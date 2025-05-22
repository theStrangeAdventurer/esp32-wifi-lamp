import { render, h } from 'preact';
import { useState, useRef, useEffect } from 'preact/hooks';

import './styles.css';

// Добавляем в начало файла
let lastCallTime = 0;
let throttleTimeout: any = null;
let debounceTimeout: any = null;

// Оптимальные задержки (в ms)
const THROTTLE_DELAY = 200; // Макс. 5 запросов в секунду
const DEBOUNCE_DELAY = 500; // Фиксируем окончание изменения

const sendBrightness = async (brightness: number) => {
  try {
    const body = `brightness=${brightness}`;
    
    const response = await fetch('/api/control', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/x-www-form-urlencoded',
      },
      body: body,
    });

    if (!response.ok) throw new Error(`HTTP error! Status: ${response.status}`);
    
    console.log("Success:", await response.text());
  } catch (error) {
    console.error("Error:", error);
  }
};

// Оптимизированный обработчик изменений
const handleBrightnessChange = (currentValue: number) => {
  const now = Date.now();
  const roundedValue = Math.round(currentValue);

  // Троттлинг: пропускаем, если не прошло достаточно времени
  if (now - lastCallTime < THROTTLE_DELAY) {
    clearTimeout(throttleTimeout);
    throttleTimeout = setTimeout(() => {
      sendBrightness(roundedValue);
      lastCallTime = Date.now();
    }, THROTTLE_DELAY - (now - lastCallTime));
    return;
  }

  // Дебаунс: сбрасываем таймер окончания
  clearTimeout(debounceTimeout);
  debounceTimeout = setTimeout(() => {
    sendBrightness(roundedValue);
  }, DEBOUNCE_DELAY);

  lastCallTime = now;
};

const App = () => {
	const [value, setValue] = useState<number>(0);
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
				const data = await resp.json();
				console.log('initial data',  data);
				setValue(10);
			} catch (error) {
				console.error("Can't receive initial value");
				setValue(10);
			}
		};

		fetchInitialValue();
	}, []);

	useEffect(() => {
		// if (typeof value === 'number')
		handleBrightnessChange(value);
		// const sendBrightness = async () => {
		// 	try {
		// 		const brightness = Math.round(value); // Округляем значение яркости
		// 		const body = `brightness=${brightness}`; // Формат application/x-www-form-urlencoded
		//
		// 		const response = await fetch('/api/control', {
		// 			method: 'POST',
		// 			headers: {
		// 				'Content-Type': 'application/x-www-form-urlencoded',
		// 			},
		// 			body: body,
		// 		});
		//
		// 		if (!response.ok) {
		// 			throw new Error(`HTTP error! Status: ${response.status}`);
		// 		}
		//
		// 		const result = await response.text();
		// 		console.log("Result: ", result);
		// 	} catch (error) {
		// 		console.error("Error while sending brightness", error);
		// 	}
		// };
		// sendBrightness();
	}, [value]);

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
