var
    _ = require('lodash'),
    fs = require('fs'),
    http = require('http'),
	path = require('path'),
    express = require('express'),
    bodyParser = require('body-parser'),
    methodOverride = require('method-override'),
    lessMiddleware = require('less-middleware'),

    disco = require('../../../common').disco,
    console = require('clim')()

    controllerConfig = (require('../../../config').cn || { }),
    httpConfig = (controllerConfig ? controllerConfig.http  : { }),
    wsConfig   = (controllerConfig ? controllerConfig.ws    : { })
    exposedHeaders =
            'X-Greyhound-Num-Points,' +
            'X-Greyhound-Read-ID,' +
            'X-Greyhound-Raster-Meta'
    ;


if (
        httpConfig.headers &&
        httpConfig.headers['Access-Control-Expose-Headers'])
{
    exposedHeaders +=
        ',' +
        httpConfig.headers['Access-Control-Expose-Headers'];
    delete httpConfig.headers['Access-Control-Expose-Headers'];
}

(function() {
    'use strict';

    var HttpHandler = function(controller, port) {
        this.controller = controller;
        this.port = port;
    }

    HttpHandler.prototype.start = function() {
        var self = this;
        var app = express();

        app.use(express.logger('dev'));
        app.use(bodyParser.json());
        app.use(bodyParser.urlencoded({ extended: true }));
        app.use(methodOverride());

        // Set the x-powered-by header
        app.use(function(req, res, next) {
            Object.keys(httpConfig.headers).map(function(key) {
                res.header(key, httpConfig.headers[key]);
            });
            res.header('X-powered-by', 'Hobu, Inc.');
            res.header('Access-Control-Expose-Headers', exposedHeaders);
            res.header('Access-Control-Allow-Headers', 'Content-Type');
            next();
        });

        // development only
        if (app.get('env') == 'development') {
            app.use(express.errorHandler());
        }

        app.use(app.router);

        if (httpConfig.enableStaticServe) {
            app.set('views', __dirname + '/static/views');
            app.set('view engine', 'jade');

            var publicDir = '/static/public';
            app.use(lessMiddleware(path.join(__dirname, publicDir)));
            app.use(express.static(__dirname + publicDir));

            app.get('/ws/:pipelineId', function(req, res) {
                res.render('wsView');
            });

            app.get('/http/:pipelineId', function(req, res) {
                res.render('httpView');
            });
        }

        registerCommands(self.controller, app);

        var server = http.createServer(app);
        var port = httpConfig.port || 8081;

        disco.register('web', port, function(err, service) {
            if (err) return console.log("Failed to register service:", err);

            server.listen(service.port, function () {
                console.log('HTTP server running on port ' + port);
            });
        });
    }

    var extend = function(err, response, command) {
        var common = {
            status: !err,
            command: command
        }

        if (err) common['reason'] = err.message;

        return _.extend(response || { }, common);
    }

    var objectify = function(obj, key) {
        if (obj[key]) {
            obj[key] = JSON.parse(obj[key]);
        }
    }

    var registerCommands = function(controller, app) {
        app.get('/pipeline/:pipeline/numPoints', function(req, res) {
            controller.numPoints(req.params.pipeline, function(err, data) {
                res.json(extend(err, data, 'numPoints'));
            });
        });

        app.get('/pipeline/:pipeline/schema', function(req, res) {
            controller.schema(req.params.pipeline, function(err, data) {
                res.json(extend(err, data, 'schema'));
            });
        });

        app.get('/pipeline/:pipeline/stats', function(req, res) {
            controller.stats(req.params.pipeline, function(err, data) {
                res.json(extend(err, data, 'stats'));
            });
        });

        app.get('/pipeline/:pipeline/srs', function(req, res) {
            controller.srs(req.params.pipeline, function(err, data) {
                res.json(extend(err, data, 'srs'));
            });
        });

        app.get('/pipeline/:pipeline/fills', function(req, res) {
            controller.fills(req.params.pipeline, function(err, data) {
                res.json(extend(err, data, 'fills'));
            });
        });

        app.post('/pipeline/:pipeline/serialize', function(req, res) {
            controller.serialize(req.params.pipeline, function(err, data) {
                res.json(extend(err, data, 'serialize'));
            });
        });

        app.delete('/pipeline/:pipeline/readId/:readId', function(req, res) {
            controller.cancel(
                req.params.pipeline,
                req.params.readId,
                function(err, data) {
                    res.json(extend(err, data, 'cancel'));
                }
            );
        });

        app.get('/pipeline/:pipeline/read', function(req, res) {
            var params = req.query;

            objectify(params, 'bbox');
            objectify(params, 'schema');
            objectify(params, 'resolution');

            controller.read(
                req.params.pipeline,
                params,
                function(err, shRes) {
                    if (err) return res.json(500, err);

                    res.header('X-Greyhound-Num-Points', shRes.numPoints);
                    res.header('X-Greyhound-Read-ID', shRes.readId);
                    res.header('Content-Type', 'application/octet-stream');

                    if (shRes.rasterMeta) {
                        res.header(
                            'X-Greyhound-Raster-Meta',
                            JSON.stringify(shRes.rasterMeta));
                    }
                },
                function(data) { res.write(data); },
                function() { res.end(); }
            );
        });
    }

    module.exports.HttpHandler = HttpHandler;
})();

