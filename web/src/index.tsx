import React from 'react';
import ReactDOM from 'react-dom/client';
import './styles.css';

const App = (props: { message: string }) => {
	return (
		<h3 className="app">{props.message}</h3>
	);
}

const root = ReactDOM.createRoot(
	document.getElementById('root') as HTMLElement
);

root.render(
	<React.StrictMode>
		<App message="Hello, TypeScript with React!" />
	</React.StrictMode>
);
