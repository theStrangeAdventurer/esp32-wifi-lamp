const fs = require('fs');
const path = require('path');
const cheerio = require('cheerio');

// Установите cheerio если нет: npm install cheerio
// npm install cheerio --save-dev

// 1. Читаем исходные файлы
const htmlTemplate = fs.readFileSync(path.join(__dirname, 'index.html'), 'utf8');
const bundleJS = fs.readFileSync(path.join(__dirname, 'dist', 'bundle.js'), 'utf8');

// 2. Загружаем HTML через cheerio для манипуляций
const $ = cheerio.load(htmlTemplate);

// 3. Заменяем script и style
$('script#bundle').remove();
$('body').append(`<script>${bundleJS}</script>`);

// 4. Получаем результат
const resultHtml = $.html();

// 5. Сохраняем
fs.writeFileSync(path.join(__dirname, 'dist', 'index.html'), resultHtml);
