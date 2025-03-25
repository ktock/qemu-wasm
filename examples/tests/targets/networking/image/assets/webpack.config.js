const path = require('path');

module.exports = {
    mode: 'development',
    entry: {
	'stack-worker':  './stack-worker.js',
	'stack':  './stack.js',
    },
    output: {
        path: path.resolve(__dirname, 'dist'),
        filename: '[name].js',
        library: {
            name: 'Stack',
            type: 'var',
        },
    },
    module: {
	rules: [
	    {
		test: /\.ts$/,
		exclude: /node_modules/,
		use: [{
		    loader: 'ts-loader'
		}]
	    },
        ]
    },
    resolve: {
	mainFields: ['browser', 'module', 'main'], // look for `browser` entry point in imported node modules
	extensions: ['.ts', '.js'], // support ts-files and js-files
	alias: {
	    // provides alternate implementation for node module and source files
	}
    }
}
