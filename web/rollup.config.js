import resolve from '@rollup/plugin-node-resolve';
import commonjs from '@rollup/plugin-commonjs';
import babel from '@rollup/plugin-babel';
import typescript from '@rollup/plugin-typescript';
import postcss from 'rollup-plugin-postcss';
import html from 'rollup-plugin-html';

export default {
	input: 'src/index.tsx',
	output: {
		file: 'dist/bundle.js',
		format: 'iife',
		name: 'app'
	},
	plugins: [
		// replace({
		// 	'process.env.NODE_ENV': JSON.stringify('production')
		// }),
		html({
			include: '**/*.html',
			htmlMinifierOptions: {
				collapseWhitespace: true,
				collapseBooleanAttributes: true,
				conservativeCollapse: true,
				minifyJS: false
			}
		}),
		postcss({
			extract: false,
			inject: true,
			minimize: false,
			modules: false
		}),
		resolve(),
		commonjs({
			include: /node_modules/
		}),
		typescript({
			// jsx: 'react',
			tsconfig: './tsconfig.json'
		}),
		babel({
			babelHelpers: 'bundled',
			presets: [
				'@babel/preset-env',
				// '@babel/preset-react',
				'@babel/preset-typescript'
			],
			extensions: ['.js', '.jsx', '.ts', '.tsx']
		}),
		// terser()
	]
};

