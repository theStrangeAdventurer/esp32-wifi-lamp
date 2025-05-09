import { render, createElement } from 'preact';
import './styles.css';

const App = (props: { message: string }) => {
	return (
		<h3 className="app">{props.message}</h3>
	);
}
render(<App message="Hello world" />, document.getElementById('root') as HTMLElement);
