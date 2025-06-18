#ifndef HTML_PAGE_H
#define HTML_PAGE_H

const char htmlPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="es">

<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Subir Archivos</title>
    <style>
        /* Estilo global de la página */
        body {
            font-family: Arial, sans-serif;
            margin: 0;
            padding: 0;
            background-color: #f4f4f9;
            color: #333;
        }

        h1,
        h2 {
            text-align: center;
            color: #444;
        }

        /* Formulario de subida */
        form {
            display: flex;
            justify-content: center;
            margin-top: 30px;
        }

        input[type="file"] {
            padding: 10px;
            font-size: 16px;
            margin-right: 10px;
            border: 2px solid #ddd;
            border-radius: 5px;
        }

        input[type="submit"] {
            padding: 10px 20px;
            font-size: 16px;
            cursor: pointer;
            background-color: #28a745;
            color: white;
            border: none;
            border-radius: 5px;
        }

        input[type="submit"]:hover {
            background-color: #218838;
        }

        /* Estilo para el contenedor de la vista previa */
        .preview {
            display: flex;
            align-items: center;
            margin-bottom: 10px;
            padding: 10px;
            background-color: #fff;
            border-radius: 5px;
            box-shadow: 0 2px 5px rgba(0, 0, 0, 0.1);
            margin: 5px 0;
        }

        .preview img {
            width: 50px;
            height: 50px;
            object-fit: contain;
            margin-right: 10px;
            border-radius: 5px;
        }

        /* Estilo para el botón de eliminación */
        .delete-btn {
            margin-left: 10px;
            padding: 5px;
            background-color: red;
            color: white;
            border: none;
            cursor: pointer;
            border-radius: 5px;
            font-size: 14px;
        }

        .delete-btn:hover {
            background-color: darkred;
        }

        /* Estilo para la lista de archivos */
        #fileList {
            list-style-type: none;
            padding: 0;
            margin-top: 30px;
        }

        /* Estilo de la información de almacenamiento */
        #storageInfo {
            background-color: #fff;
            padding: 15px;
            border-radius: 5px;
            box-shadow: 0 2px 5px rgba(0, 0, 0, 0.1);
            text-align: center;
            margin-top: 20px;
        }

        /* Estilo cuando no hay archivos */
        .no-files {
            text-align: center;
            color: #999;
            font-style: italic;
        }
    </style>
    <script>
        // Función para manejar la subida de archivos
        function handleFileUpload(event) {
            event.preventDefault();
            let formData = new FormData();
            let fileInput = document.getElementById("fileInput");
            let file = fileInput.files[0];

            if (!file) {
                alert("Por favor selecciona un archivo.");
                return;
            }

            if (file.type.startsWith('image/')) {
                console.log('Es una imagen:', file.name);
            } else {
                alert('Por favor selecciona un archivo de imagen.');
                event.target.value = '';
            }

            formData.append("image", file);

            fetch('/upload', {
                method: 'POST',
                body: formData
            })
                .then(response => response.json())
                .then(data => {
                    alert(data.message);
                    event.target.value = '';  // Limpiar el input
                    loadFiles();  // Recargar la lista de archivos después de la subida
                })
                .catch(error => console.error("Error:", error));
        }

        // Función para cargar la lista de archivos
        function loadFiles() {
            fetch('/files')
                .then(response => response.json())
                .then(files => {
                    let fileList = document.getElementById("fileList");
                    fileList.innerHTML = "";  // Limpiar lista antes de agregar los nuevos archivos

                    if (files.length === 0) {
                        fileList.innerHTML = '<li class="no-files">No hay archivos subidos.</li>';
                    }

                    files.forEach(file => {
                        let li = document.createElement("li");
                        let imgElement = document.createElement("img");
                        let deleteButton = document.createElement("button");

                        li.className = "preview";
                        imgElement.src = `/view?file=${file.name}`;
                        imgElement.alt = file.name;

                        deleteButton.textContent = 'Eliminar';
                        deleteButton.className = 'delete-btn';
                        deleteButton.onclick = function () {
                            deleteFile(file.name);
                        };

                        li.appendChild(imgElement);
                        li.appendChild(document.createTextNode(` ${file.name} - ${formatSize(file.size)}`));
                        li.appendChild(deleteButton);
                        fileList.appendChild(li);
                    });
                });

            // Cargar el estado de almacenamiento
            fetch('/storage')
                .then(response => response.json())
                .then(data => {
                    let storageInfo = document.getElementById("storageInfo");
                    storageInfo.innerHTML = `<strong>Espacio Total:</strong> ${formatSize(data.total)} | 
                                         <strong>Usado:</strong> ${formatSize(data.used)} | 
                                         <strong>Libre:</strong> ${formatSize(data.free)} | 
                                         <strong>Utilizado:</strong> ${data.percentage}%`;
                });
        }

        // Función para eliminar un archivo usando FormData
        function deleteFile(filename) {
            // Crear un objeto FormData
            const formData = new FormData();

            // Agregar el nombre del archivo al FormData
            formData.append('fileDelete', filename); // 'delete_file' debe coincidir con el nombre que maneja el servidor

            // Enviar la solicitud utilizando fetch
            fetch('/delete', {
                method: 'POST',
                body: formData  // Enviar el FormData como el cuerpo de la solicitud
            })
                .then(response => response.json())  // Convertir la respuesta a JSON
                .then(data => {
                    alert(data.message);  // Mostrar mensaje de éxito o error
                    loadFiles();  // Recargar la lista de archivos después de eliminar
                })
                .catch(error => {
                    console.error('Error al eliminar el archivo:', error);
                });
        }

        // Función para convertir bytes a un tamaño legible
        function formatSize(bytes) {
            if (bytes < 1024) return bytes + " B";
            if (bytes < 1048576) return (bytes / 1024).toFixed(2) + " KB";
            if (bytes < 1073741824) return (bytes / 1048576).toFixed(2) + " MB";
            return (bytes / 1073741824).toFixed(2) + " GB";
        }

        // Cargar los archivos y el estado de almacenamiento cuando la página cargue
        window.onload = function () {
            loadFiles();
        };
    </script>
</head>

<body>
    <h1>Subir Archivo</h1>
    <form onsubmit="handleFileUpload(event)">
        <input type="file" id="fileInput" accept="image/*" required><br><br>
        <input type="submit" value="Subir">
    </form>
    <h2>Estado de Almacenamiento</h2>
    <p id="storageInfo"></p>
    <h2>Archivos Subidos</h2>
    <ul id="fileList"></ul>
</body>

</html>
)rawliteral";

#endif  // HTML_PAGE_H
