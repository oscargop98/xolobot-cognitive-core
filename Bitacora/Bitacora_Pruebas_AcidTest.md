# Bitácora de Pruebas "Acid Test" y Traspaso de Entorno (Onboarding)

**Periodo:** 18 de septiembre - 28 de septiembre de 2026
**Objetivo:** Validar la reproducibilidad del entorno `xolobot-cognitive-core` en hardware independiente y documentar el traspaso del proyecto a nuevos alumnos.

---

## 1. Pruebas de Estrés en Hardware Independiente (SS-Neuro)

Para garantizar que el proyecto no dependiera exclusivamente del equipo principal de desarrollo, se habilitó una computadora de pruebas con recursos limitados.

Se realizó una instalación limpia de Ubuntu 24.04 en una laptop desensamblada, a la cual se le asignó el nombre de host `ss-neuro` en honor al proyecto. Posteriormente, se procedió con la instalación de ROS 2 Jazzy Jalisco configurando las variables de entorno (`locales`) y los repositorios correspondientes. El proceso incluyó la descarga y desempaquetado de todas las dependencias necesarias.

Dado el hardware de la máquina, la compilación completa de los paquetes de ROS 2 representó una prueba de estrés significativa; el proceso tomó entre 2 y 3 horas, manteniendo la CPU trabajando a su máxima capacidad. A pesar de los detalles de rendimiento gráfico esperados por las limitaciones de la máquina, el sistema operativo y el entorno base se instalaron con éxito.

### Evidencia de Configuración
![Laptop de Pruebas](assets/Prueaba_01.png)
![Estrés de CPU durante colcon build](assets/Prueba_02.png)

---

## 2. Validación de Clonación y Ejecución (Acid Test)

Una vez preparado el equipo `ss-neuro`, se simuló el flujo de trabajo de un usuario nuevo siguiendo la documentación del repositorio oficial:

1. Se creó el workspace temporal y se clonó el repositorio `xolobot-cognitive-core` de forma exitosa.
2. Se recompilaron los paquetes y se lanzaron los nodos del proyecto. La simulación en Gazebo Harmonic logró renderizar el brazo robótico, la lata y el entorno físico sin errores.
3. Se validó la inyección de los alias de ejecución, confirmando que comandos como `xolo_view` y `xolo_brain` interactúan correctamente con la simulación levantada y se sincronizan con los repositorios en la nube.

### Evidencia de Ejecución
![Clonación del Repositorio](assets/Instalacion_05.png)
![Entorno de Gazebo Ejecutándose](assets/Instalacion_07.png)

---

## 3. Traspaso a Nuevos Integrantes (Diego Vazquez)

**Fecha de inicio:** 28 de septiembre de 2026

En coordinación con la Dra. Montserrat, se inició el traspaso formal del entorno actualizado al alumno Diego Vazquez, quien continuará con la siguiente fase operativa del proyecto.

- **Diagnóstico y Prerrequisitos:** Se detectó que Diego contaba con Ubuntu 22. Se acordó la creación de una partición para instalar Ubuntu 24.04, garantizando compatibilidad nativa con ROS 2 Jazzy.
- **Entrega de Repositorio:** Se le proporcionó la URL oficial del repositorio junto con el enlace a la documentación oficial de instalación de ROS 2 Jazzy.
- **Metodología de Onboarding:** Se le instruyó leer primero el archivo `README.md` maestro para familiarizarse con la arquitectura (Opción Nativa vs. Docker) y los alias del panel de control antes de ejecutar comandos.
- **Retroalimentación:** Las dudas que surjan durante la instalación de Diego serán monitoreadas para iterar y robustecer aún más la documentación del repositorio para futuras generaciones.
