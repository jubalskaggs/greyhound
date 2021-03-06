// A simple websocket client which requests points from the point-serve service
//
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>

#include <boost/algorithm/string.hpp>
#include <json/json.h>

#include <iostream>

class WebSocketClient {
    typedef websocketpp::client<websocketpp::config::asio_client> client;
    typedef websocketpp::config::asio_client::message_type::ptr message_ptr;
    typedef WebSocketClient this_type;

public:
    WebSocketClient(const std::string& uri)
        : uri_(uri)
        , in_exchange_(false)
        , client_()
    {
        using websocketpp::lib::placeholders::_1;
        using websocketpp::lib::placeholders::_2;
        using websocketpp::lib::bind;

        // We expect there to be a lot of errors, so suppress them
        client_.clear_access_channels(websocketpp::log::alevel::all);
        client_.clear_error_channels(websocketpp::log::elevel::all);

        // Initialize ASIO
        client_.init_asio();
    }

    template<typename PointsCountHandler>
    void GetPointsCount(const std::string& id, PointsCountHandler handler) {
        Json::Value v;
        v["command"] = "numPoints";
        v["pipeline"] = id;

        do_exchange(v, [handler](const Json::Value& r) {
            if (r["status"] == 1)
                handler(r["numPoints"].asInt());
            else
                handler(-1);
        });
    }

    template<typename ReadHandler, typename DataHandler>
    void Read(
            const std::string& id,
            ReadHandler handler,
            DataHandler dhandler,
            int offset = 0,
            int count = -1) {
        Json::Value v;
        v["command"] = "read";
        v["pipeline"] = id;
        v["start"] = offset;
        if (count != -1) v["count"] = count;

        do_exchange_with_swap(v, [this, handler](const Json::Value& r) {
            if (r["status"] == 1) {
                // std::cout << r.toStyledString() << std::endl;

                int npoints = r["numPoints"].asInt();
                int nbytes = r["numBytes"].asInt();

                handler(npoints, nbytes);
            }
            else
                handler(0, 0);
        }, [dhandler](message_ptr msg) {
            if (msg->get_opcode() == websocketpp::frame::opcode::binary)
                dhandler(msg->get_payload());
        });
    }

    void Run() {
        client_.run();
    }

private:
    void send(websocketpp::connection_hdl hdl, const Json::Value& v) {
        client_.send(
                hdl,
                v.toStyledString(),
                websocketpp::frame::opcode::text);
    }

    template<typename F>
    void do_exchange(const Json::Value& v, F handler) {
        if (in_exchange_) {
            std::cerr << "Exchange failed" << std::endl;
            return;     // only one exchange in progress at a time
        }

        in_exchange_ = true;
        client_.set_open_handler([this, v](websocketpp::connection_hdl hdl) {
            send(hdl, v);
        });

        client_.set_message_handler(
                [this, handler](
                    websocketpp::connection_hdl hdl,
                    message_ptr msg) {
            Json::Value v;
            Json::Reader r;

            r.parse(msg->get_payload(), v);
            this->in_exchange_ = false;

            handler(v);
        });

        // getting connection and everything
        websocketpp::lib::error_code ec;
        client::connection_ptr con = client_.get_connection(uri_, ec);
        client_.connect(con);
    }

    // The following function is needed to replace the message handler right
    // away before the system gets a chance to send down another message
    // through our message handler, this is needed where the next message
    // needs to be processed through a different message handler, e.g. when
    // reading points.  The swapped function gets raw messages as a bonus.
    template<typename F, typename S>
    void do_exchange_with_swap(const Json::Value& v, F handler, S swapped) {
        if (in_exchange_) {
            std::cerr << "Exchange failed" << std::endl;
            return;     // only one exchange in progress at a time
        }

        in_exchange_ = true;
        client_.set_open_handler([this, v](websocketpp::connection_hdl hdl) {
            send(hdl, v);
        });

        swapdone_ = false;
        client_.set_message_handler(
                [this, handler, swapped](
                    websocketpp::connection_hdl hdl,
                    message_ptr msg)
        {
            Json::Value v;
            Json::Reader r;

            r.parse(msg->get_payload(), v);
            this->in_exchange_ = false;

            if (!swapdone_) {
                handler(v);
                swapdone_ = true;
            }
            else {
                swapped(msg);
            }
        });

        // getting connection and everything
        websocketpp::lib::error_code ec;
        client::connection_ptr con = client_.get_connection(uri_, ec);
        client_.connect(con);
    }

private:
    std::string uri_;
    bool in_exchange_, swapdone_;
    client client_;
};


int main(int argc, char* argv[])
{
    if (argc > 2)
    {
        std::cout << "Usage: " << argv[0] <<
            " <greyhoundPipelineId>" << std::endl;
        return 1;
    }

    WebSocketClient client("ws://localhost:8080/");
    const std::string plId =
        argc == 2 ?
            argv[1] :
            "5adcf597e3376f98471bf37816e9af2c"; // ID of sample pipeline

    int bytesToRead = 0, bytesRead = 0;

    client.GetPointsCount(
            plId,
            [&client, plId, &bytesToRead, &bytesRead](int count)
    {
        std::cout << "Pipeline has " << count << " points." << std::endl;

        client.Read(plId, [&bytesToRead](int npoints, int nbytes) {
            std::cout << "Total " << npoints << " points in " <<
                    nbytes << " bytes will arrive." << std::endl;

            bytesToRead = nbytes;

            if (nbytes == 0)
            {
                std::cout <<
                    "No points to arrive - exiting" <<
                    std::endl;

                exit(1);
            }
        },
        [plId, &client, &bytesToRead, &bytesRead](const std::string& data) {
            bytesRead += data.length();

            if (bytesRead >= bytesToRead)
            {
                std::cout << "All bytes read in." << std::endl;
                exit(0);
            }
        });
    });

    client.Run();
}

