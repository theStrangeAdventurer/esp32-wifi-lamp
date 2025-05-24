import { render, h } from 'preact';
import { useState, useRef, useEffect } from 'preact/hooks';

import './styles.css';

const App = () => {
	const [value, setValue] = useState<number>(0);
	const [locked, setLocked] = useState(false);
	const lastSetVal = useRef(0);
	const sliderRef = useRef<HTMLDivElement>(null);
	const [isDragging, setIsDragging] = useState<boolean>(false);

	// Функция для расчета значения на основе позиции касания/клика
	const calculateValue = (clientY: number) => {
		if (!sliderRef.current) return;

		const rect = sliderRef.current.getBoundingClientRect();
		const height = rect.height;
		const y = clientY - rect.top; // Позиция относительно верхней части шкалы
		const newValue = Math.max(0, Math.min(100, ((height - y) / height) * 100)); // Переводим в процент
		setValue(newValue);
	};

	// Обработчики для мыши
	const handleMouseDown = (e: MouseEvent) => {
		setIsDragging(true);
		calculateValue(e.clientY);
	};

	const handleMouseMove = (e: MouseEvent) => {
		if (isDragging) {
			calculateValue(e.clientY);
		}
	};

	const handleMouseUp = () => {
		setIsDragging(false);
	};

	// Обработчики для сенсорных устройств
	const handleTouchStart = (e: TouchEvent) => {
		setIsDragging(true);
		calculateValue(e.touches[0].clientY);
	};

	const handleTouchMove = (e: TouchEvent) => {
		if (isDragging) {
			calculateValue(e.touches[0].clientY);
		}
	};

	const handleTouchEnd = () => {
		setIsDragging(false);
	};

	useEffect(() => {
		const fetchInitialValue = async () => {
			try {
				const resp = await fetch('/api/control');
				const { data: { brightness } } = await resp.json();
				setValue(brightness);
			} catch (error) {
				console.error("Error while receive initial value", error);
				setValue(10);
			}
		};

		fetchInitialValue();
	}, []);

	useEffect(() => {
		const brightness = Math.round(value); // Округляем значение яркости

		const sendBrightness = async () => {
			setLocked(true);
			try {
				const body = `brightness=${brightness}`; // Формат application/x-www-form-urlencoded
				setLocked(true);

				const response = await fetch('/api/control', {
					method: 'POST',
					headers: {
						'Content-Type': 'application/x-www-form-urlencoded',
					},
					body: body,
				});

				if (!response.ok) {
					throw new Error(`HTTP error! Status: ${response.status}`);
				}

				const result = await response.text();
				console.log("Result: ", result);
			} catch (error) {
				console.error("Error while sending brightness", error);
			} finally {
				console.log('finally called', { brightness });
				lastSetVal.current = brightness;
				setLocked(false);
			}
		};

		if (!locked && lastSetVal.current !== brightness)
			sendBrightness();
	}, [value, locked]);

	// Добавляем глобальные обработчики для событий движения и отпускания
	useEffect(() => {
		window.addEventListener('mousemove', handleMouseMove);
		window.addEventListener('mouseup', handleMouseUp);
		window.addEventListener('touchmove', handleTouchMove);
		window.addEventListener('touchend', handleTouchEnd);

		return () => {
			window.removeEventListener('mousemove', handleMouseMove);
			window.removeEventListener('mouseup', handleMouseUp);
			window.removeEventListener('touchmove', handleTouchMove);
			window.removeEventListener('touchend', handleTouchEnd);
		};
	}, [isDragging]);

	// Генерируем 10 полосок-делений
	const markers = Array.from({ length: 10 }, (_, index) => (
		<div
			key={index}
			className="marker"
			style={{ bottom: `${(index + 1) * 10}%` }} // Распределяем полоски равномерно (10%, 20%, ..., 90%)
		/>
	));
	return (
		<div className="app-container">
			<div
				className="slider-container"
				ref={sliderRef}
				onMouseDown={handleMouseDown}
				onTouchStart={handleTouchStart}
			>
				{markers}
				<div
					className="slider-fill"
					style={{ height: `${value}%` }}
				/>
				<div className="brightness-label">
					{Math.round(value)}%
				</div>
			</div>
		</div>
	);
};

render(<App />, document.getElementById('root') as HTMLElement);
