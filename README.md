# EgoitzPineiroTools · Generador procedural de ciudades

Plugin de Unreal Engine 5.8 que genera ciudades pequeñas con el PCG Framework: red de calles, manzanas
con acera, edificios y mobiliario urbano (farolas y árboles). Los ingenieros de escenarios lo usan
desde un panel propio del editor, sin abrir ningún grafo. La misma seed y los mismos parámetros
producen siempre la misma ciudad.

## Requisitos

- Unreal Engine 5.8 y un proyecto C++ (el plugin incluye código y se compila con el proyecto).
- Visual Studio 2022 con la carga de trabajo *Desarrollo de juegos con C++*.
- El plugin **Procedural Content Generation Framework (PCG)** se activa solo: es una dependencia
  declarada en el `.uplugin`.

## Instalación

1. Copia la carpeta `EgoitzPineiroTools` en `<TuProyecto>/Plugins/`.
2. Clic derecho en el `.uproject` › *Generate Visual Studio project files*.
3. Compila en *Development Editor* y abre el proyecto.
4. Comprueba en *Edit › Plugins* que **EgoitzPineiroTools - City Generator** está activado.

No hay que modificar nada del proyecto anfitrión: el código, los grafos, los materiales y las mallas
están dentro del plugin.

## Uso

1. Abre el panel con el botón **Ciudad** de la barra de herramientas o con
   *Tools › Generador de ciudades*.
2. Ajusta los parámetros y pulsa **Generar ciudad**. La ciudad aparece centrada en el origen del nivel,
   dentro de un actor `ProceduralCity`.
3. **Limpiar ciudad** elimina por completo la ciudad: el actor y todo lo que PCG generó dentro de él.
   **Generar ciudad** también limpia la anterior antes de crear la nueva, así que nunca quedan restos.
4. Con **Regenerar al cambiar un valor** activado, la ciudad se actualiza al soltar un deslizador o al
   escribir un valor.
5. **Aleatoria** elige una seed nueva al azar.

La sección **Ciudad en el nivel** del panel muestra cuántas instancias hay de cada tipo
(malla / material) y una **huella**: un resumen numérico de la posición, el giro y la escala de todas
las instancias. Si dos generaciones tienen la misma huella, son la misma ciudad.

**Reproducir un escenario:** basta con guardar la seed y los cinco parámetros. Con ellos se reconstruye
exactamente la misma ciudad, con la misma huella.

## Parámetros

| Panel | Graph Parameter | Rango | Qué controla |
| --- | --- | --- | --- |
| Tamaño de la ciudad (m) | `CitySize` | 50 – 3000 | Lado de la ciudad cuadrada. Cruces por lado = Floor(CitySize / (BlockSize + 8)) + 1. |
| Tamaño de manzana (m) | `BlockSize` | 15 – 300 | Lado de cada manzana con su acera. Manzanas pequeñas generan más cruces. |
| Densidad (0-1) | `BuildingDensity` | 0 – 1 | Parte de las parcelas de fachada que se construyen: 0 es una zona abierta y 1, un cañón urbano. |
| Altura media (m) | `BuildingHeight` | 3 – 300 | Altura media de los edificios. |
| Variación (0-1) | `BuildingVariation` | 0 – 1 | Diferencia entre edificios: altura ±75 % y huella entre el 90 % y el 60 % de la parcela. Con 0 todos son iguales. |
| Seed | *Seed* del PCG Component | 0 – 9 999 999 | Identifica el escenario. Cambia qué parcelas se construyen, la forma y el material de cada edificio y el giro y tamaño de los árboles. |

Constantes internas (Details de cada nodo, en metros): ancho de calle 8, ancho de acera 3, altura de
acera 0.15, tamaño de parcela 20, separación del mobiliario 12 y distancia libre a los cruces 5.

## Cómo funciona

La ciudad es una pila de capas. Cada capa se calcula como puntos a partir de la anterior, y las mallas se
generan al final.

```
City Crossings ─► City Street Segments ─► calzada
      │
      └──► City Blocks ─► acera ──┬─► City Lots ─► Frontage ─► densidad ─► City Buildings ─► edificios
                                  │
                                  └─► City Sidewalk Slots ─┬─► farolas  (PropType 0)
                                                            └─► árboles  (PropType 1)
```

1. **Calles.** *City Crossings* crea una rejilla de cruces centrada en el origen. *City Street Segments*
   une cada cruce con sus vecinos de fila y de columna. Cada tramo acaba donde empieza el cruce siguiente:
   no hay solapes.
2. **Manzanas.** *City Blocks* coloca una manzana en cada celda de cuatro cruces, ocupando exactamente el
   hueco entre sus calzadas. Las manzanas se derivan de las calles, así que nunca las invaden.
3. **Edificios.** *City Lots* reparte el interior de cada manzana (sin el anillo de acera) en parcelas que
   miran a la calle. Después, con nodos estándar: *Attribute Filter* se queda con las parcelas de fachada,
   *Attribute Noise* da a cada una un `$Density` aleatorio y *Density Filter* deja pasar las que están por
   debajo de `BuildingDensity`. *City Buildings* calcula la altura y la huella; *Transform Points* añade
   un pequeño desplazamiento, *Self Pruning* elimina cualquier solape y *Static Mesh Spawner* elige
   entre tres materiales.
4. **Mobiliario.** *City Sidewalk Slots* crea puntos por el centro de las aceras, orientados hacia la calle
   y lejos de los cruces, con un atributo `PropType` que alterna 0 y 1. Cada tipo de mobiliario es un
   subgrafo que filtra su `PropType`: farolas (`SG_StreetLamps`) y árboles (`SG_Trees`, con giro y escala
   aleatorios).

Cada sistema es un subgrafo y `PCG_City` solo los cablea. El detalle nodo a nodo está en
[`Docs/Grafos.md`](Docs/Grafos.md).

### Nodos C++ propios

| Nodo | Entradas | Salida | Atributos |
| --- | --- | --- | --- |
| City Crossings | — | `Crossings` | `Col`, `Row` |
| City Street Segments | `Crossings` | `Segments` | `Axis` |
| City Blocks | `Crossings` | `Blocks` | `BlockID`, `Col`, `Row` |
| City Lots | `Blocks` | `Lots` | `BlockID`, `Frontage` |
| City Buildings | `Lots` | `Buildings` | `Height`, `Floors` |
| City Sidewalk Slots | `Blocks`, `Crossings` | `Slots` | `PropType`, `Side`, `BlockID` |

Todos los puntos de calzada, acera y edificio usan el mismo convenio: el transform lleva la posición, el
giro y el tamaño (como escala), y las bounds son un cubo de 1 m. Así un *Static Mesh Spawner* con
`/Engine/BasicShapes/Cube` dibuja cada punto a su tamaño real, y *Self Pruning* o *Debug* ven las cajas
reales.

### Determinismo

No hay azar fuera de PCG. Cada punto tiene un `$Seed` calculado a partir de su posición, y todos los
nodos aleatorios (*Attribute Noise*, *Transform Points*, *Static Mesh Spawner* y *City Buildings*) lo
combinan con la seed del PCG Component. Con la misma seed, cada parcela saca siempre los mismos
números. Con otra seed, las reglas son las mismas pero los valores cambian.

### Limpieza

Todo lo que genera la herramienta vive dentro de un actor con el tag `EPT_CityGenerated`. Limpiar llama
a `CleanupLocal` en su PCG Component y después destruye el actor. Generar limpia siempre antes de crear
la ciudad nueva.

### Panel

Pestaña Slate del módulo `EgoitzPineiroTools`. Escribe los valores en los *Graph Parameters* de
`PCG_City` con `SetGraphParameter`, pone la seed en el PCG Component y llama a `GenerateLocal`. Si
falta un parámetro en el grafo o tiene otro tipo, el panel lo indica.

## Estructura del plugin

```
EgoitzPineiroTools/
├── EgoitzPineiroTools.uplugin
├── Resources/Icon128.png
├── Content/City/                      grafos, materiales y mallas (ver Docs/Grafos.md)
├── Docs/Grafos.md                     los grafos nodo a nodo
└── Source/
    ├── EgoitzPineiroToolsRuntime/     Runtime: nodos PCG y actor de la ciudad
    │   ├── Public/CityGeneratorActor.h
    │   ├── Public/CityPCG/            un nodo por archivo + nombres de pines y atributos
    │   └── Private/CityPCG/           implementación y utilidades comunes
    └── EgoitzPineiroTools/            Editor: panel, menú y botón
        └── Private/
            ├── CityGeneratorTool      generar, limpiar y estadísticas (sin interfaz)
            └── SCityGeneratorPanel    la interfaz Slate
```

Los nodos están en un módulo **Runtime** para que existan también en una build empaquetada. El panel está
en un módulo **Editor**, que nunca llega a la build final.

## Ampliar

- **Un tipo de mobiliario nuevo:** sube `NumPropTypes` en *City Sidewalk Slots* y añade un subgrafo que
  filtre el nuevo `PropType`. Los demás no cambian. Pasos en `Docs/Grafos.md`.
- **Otros edificios:** añade entradas al *Static Mesh Spawner* de `SG_Buildings`.
- **Otra capa:** escribe un nodo nuevo partiendo de cualquiera de `Source/EgoitzPineiroToolsRuntime/Public/CityPCG`.
  Las utilidades de `CityPCGUtils.h` leen y crean puntos con el mismo convenio de cajas.

## Limitaciones conocidas

- La ciudad es una retícula regular, centrada en el origen del mundo. Mover el actor no mueve la ciudad.
- La generación se hace en el editor; la ciudad generada se guarda con el nivel.
