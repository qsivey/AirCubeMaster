# AirCubeMaster
<!DOCTYPE html>
<html lang="ru">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>AirCube - Music is everywhere</title>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
        }
        
        body {
            background-color: #0d1117;
            color: #c9d1d9;
            line-height: 1.6;
            padding: 2rem;
            max-width: 900px;
            margin: 0 auto;
        }
        
        .header {
            text-align: center;
            margin-bottom: 2.5rem;
        }
        
        .project-section {
            display: flex;
            align-items: flex-start;
            margin-bottom: 2.5rem;
            background: linear-gradient(145deg, #161b22, #0d1117);
            border-radius: 12px;
            padding: 1.5rem;
            box-shadow: 0 8px 16px rgba(0, 0, 0, 0.2);
        }
        
        .icon {
            font-size: 3rem;
            margin-right: 1.5rem;
            color: #58a6ff;
            min-width: 60px;
            text-align: center;
        }
        
        .content {
            flex: 1;
        }
        
        h1 {
            color: #58a6ff;
            margin-bottom: 1rem;
            font-size: 2.2rem;
        }
        
        h2 {
            color: #f78166;
            margin: 1.8rem 0 1rem;
            font-size: 1.5rem;
            border-bottom: 1px solid #30363d;
            padding-bottom: 0.5rem;
        }
        
        p {
            margin-bottom: 1rem;
            font-size: 1.05rem;
        }
        
        ul {
            list-style-type: none;
            padding-left: 0.5rem;
        }
        
        li {
            margin-bottom: 0.8rem;
            padding-left: 1.5rem;
            position: relative;
        }
        
        li:before {
            content: "•";
            color: #58a6ff;
            font-weight: bold;
            position: absolute;
            left: 0;
            font-size: 1.2rem;
        }
        
        .features {
            background: linear-gradient(145deg, #161b22, #0d1117);
            border-radius: 12px;
            padding: 1.5rem;
            box-shadow: 0 8px 16px rgba(0, 0, 0, 0.2);
        }
        
        @media (max-width: 768px) {
            .project-section {
                flex-direction: column;
                align-items: center;
                text-align: center;
            }
            
            .icon {
                margin-right: 0;
                margin-bottom: 1rem;
            }
            
            li {
                text-align: left;
            }
        }
    </style>
</head>
<body>
    <div class="header">
        <img src="https://readme-typing-svg.herokuapp.com?font=Fira+Code&size=24&duration=4000&pause=500&center=true&vCenter=true&width=435&lines=AirCube;Music+is+everywhere" alt="SVG" />
    </div>
    
    <div class="project-section">
        <div class="icon">🎵</div>
        <div class="content">
            <h1>AirCube</h1>
            <p>AirCube - продукт, который революционизирует способ взаимодействия с музыкой. Не нужно возиться с проводами, разбираться в сложных настройках и т.д. Просто нажмите на одну кнопку и музыка сразу будет играть. Элегантный дизайн в форме куба создан для непревзойденного музыкального опыта в любом помещении.</p>
            <p>С AirCube вы можете наслаждаться кристально чистым звуком, адаптирующимся под акустические особенности вашего пространства, и управлять всем с помощью простого и интуитивного интерфейса.</p>
        </div>
    </div>
    
    <div class="features">
        <h2>Возможности проекта</h2>
        <ul>
            <li>Адаптивная акустическая система, автоматически подстраивающаяся под особенности помещения</li>
            <li>Поддержка всех популярных музыкальных сервисов и потоковых платформ</li>
            <li>Многокомнатное аудио: синхронизация нескольких устройств AirCube по всему дому</li>
            <li>Высокое качество звука с поддержкой форматов Hi-Res Audio</li>
            <li>Стильный и минималистичный дизайн, который гармонично вписывается в любой интерьер</li>
            <li>Простое управление через мобильное приложение с интуитивным интерфейсом</li>
        </ul>
    </div>
</body>
</html>
