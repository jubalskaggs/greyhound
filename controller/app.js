process.title = 'gh_cn';

process.on('uncaughtException', function(err) {
    console.log('Caught at top level: ' + err);
});

var
    console = require('clim')(),

    Controller = require('./controller').Controller,
    WsHandler = require('./interfaces/ws/handler').WsHandler,
    HttpHandler = require('./interfaces/http/handler').HttpHandler,

    config = (require('../config').cn || { }),
    globalConfig = (require('../config').global || { })
    ;

if (config.enable === false) {
    process.exit(globalConfig.quitForeverExitCode || 42);
}

var controller = new Controller();

process.nextTick(function() {
    if (config.hasOwnProperty('ws') && config.ws.enable) {
        var wsHandler = new WsHandler(controller, config.ws.port);
        wsHandler.start();
    }

    if (config.hasOwnProperty('http') && config.http.enable) {
        var httpHandler = new HttpHandler(controller, config.ws.port);
        httpHandler.start();
    }
});

