# Grafos PCG de la ciudad

Este documento describe, nodo a nodo, los grafos de `Content/CityGenerator`. Los monta el código
(`Source/EgoitzPineiroToolsEditor/Private/CityContent.cpp › BuildGraphs`), así que este documento sirve
para entenderlos antes de ampliarlos. No hace falta crearlos a mano.

```
Content/CityGenerator/
├── PCG_City                 grafo principal: solo ejecuta los subgrafos
├── Subgraphs/
│   ├── SG_Streets           calzada
│   ├── SG_Blocks            manzanas (acera)
│   ├── SG_Buildings         parcelas, densidad y edificios
│   ├── SG_StreetLamps       farolas
│   ├── SG_Trees             árboles de las aceras
│   └── SG_Parks             césped, árboles y arbustos de los parques
├── Materials/               M_CityColor y sus instancias MI_*
└── Meshes/                  SM_StreetLamp, SM_Tree, SM_Bush
```

## Convenio de puntos

Calles, manzanas, parcelas y edificios son puntos que se leen como cajas: el transform lleva la posición, el
giro y el tamaño (como escala), y las bounds son un cubo de 1 m. Así, un *Static Mesh Spawner* con
`/Engine/BasicShapes/Cube` dibuja cada punto a su tamaño real, y *Self Pruning* ve las cajas reales. Los
puntos de mobiliario tienen escala 1, para que las mallas conserven su tamaño.

## Nodos C++ propios

| Nodo | Entradas | Salidas | Atributos | Parámetros conectados |
| --- | --- | --- | --- | --- |
| City Street Grid | — | `Roads`, `Blocks`, `UrbanBlocks`, `ParkBlocks` | Roads: `RoadKind`, `Axis`, `Col`, `Row`. Manzanas: `BlockID`, `Col`, `Row`, `Park` | `CitySize`, `BlockSize`, `ParkRatio` |
| City Lots | `Blocks` | `Lots` | `BlockID`, `Side`, `$Density` | `BuildingDensity` |
| City Buildings | `Lots` | `Buildings` | `Height`, `Floors` | `BuildingHeight`, `BuildingVariation` |
| City Sidewalk Points | `Blocks` | `Points` | `BlockID`, `Side` | — |
| City Park | `Blocks` | `Lawns`, `Plants` | Plants: `BlockID` | `TreeDensity` ← `ParkTreeDensity`, `ScaleVariation` ← `ParkTreeScaleVariation` |

Los parámetros se conectan a las propiedades del mismo nombre (pines de override de PCG). Las demás
propiedades de cada nodo son constantes que se pueden cambiar en *Details*.

## Subgrafos

Todos los subgrafos empiezan igual: dos *Get Graph Parameter* (`CitySize`, `BlockSize`) conectados a un
*City Street Grid*. Los que usan `UrbanBlocks` o `ParkBlocks` le conectan también `ParkRatio`. Cada subgrafo
vuelve a calcular la retícula a partir de los parámetros: no recibe datos de otros subgrafos, así que se
puede quitar o añadir uno sin tocar los demás.

*City Street Grid* decide qué manzanas son parque con un número aleatorio por manzana, calculado a partir de
la seed y de su posición. Por eso todos los subgrafos coinciden. (No cambies la *Seed* propia de estos
nodos en *Details*: tiene que ser la misma en todos.)

### SG_Streets
`City Street Grid.Roads` → *Static Mesh Spawner* (Cube, `MI_Road`).

### SG_Blocks
`City Street Grid.Blocks` → *Static Mesh Spawner* (Cube, `MI_Sidewalk`). Cada manzana es una losa de
15 cm sobre la calzada: la acera.

### SG_Buildings
Parámetros extra: `ParkRatio`, `BuildingDensity`, `BuildingHeight` y `BuildingVariation`.

1. `City Street Grid.UrbanBlocks` → **City Lots** (los parques no se edifican). Dentro del anillo de acera (3 m), una franja de 18 m de fondo
   a lo largo de los cuatro lados se corta en parcelas de unos 15 m de frente. Las esquinas son de las filas
   norte y sur, así que no hay solapes. Cada parcela recibe
   `$Density = 1 − 0.5 · R / BuildingDensity`, con `R` aleatorio en [0, 1) a partir de la seed.
2. **Density Filter** (0.5 – 1): deja pasar las parcelas con `R ≤ BuildingDensity`, es decir, ese
   porcentaje de parcelas.
3. **City Buildings**: altura `BuildingHeight · (1 ± 0.75 · BuildingVariation)` (mínimo 3 m), huella del
   85 % de la parcela menos hasta un 30 % · `BuildingVariation` por eje, y un desplazamiento dentro de la
   parcela que nunca la saca de ella.
4. **Transform Points**: giro aleatorio de ±4°.
5. **Self Pruning**: elimina los edificios que se solapen.
6. **Static Mesh Spawner**: Cube con `MI_Building_A`, `_B` o `_C` (elección ponderada, con la seed).

### SG_StreetLamps
`City Street Grid.Blocks` → **City Sidewalk Points** (`Spacing` 24 m, `Phase` 0, `CurbOffset` 0.6 m,
`CornerClearance` 6 m) → *Static Mesh Spawner* (`SM_StreetLamp`).

### SG_Trees
`City Street Grid.Blocks` → **City Sidewalk Points** (`Spacing` 24 m, `Phase` 0.5, `CurbOffset` 1.4 m,
`CornerClearance` 6 m) → **Transform Points** (giro 0 – 360°, escala uniforme 0.8 – 1.25) →
*Static Mesh Spawner* (`SM_Tree`). Con `Phase` 0.5 los árboles quedan a medio camino entre dos farolas.

### SG_Parks
Parámetros extra: `ParkRatio`, `ParkTreeDensity` y `ParkTreeScaleVariation`.

1. `City Street Grid.ParkBlocks` → **City Park**.
2. `Lawns` → *Static Mesh Spawner* (Cube, `MI_Grass`): el césped, 10 cm por encima de la acera, dentro del
   anillo de acera.
3. `Plants` (un hueco cada 6 m, desplazado hasta un 35 %, con probabilidad `ParkTreeDensity` y escala
   `1 ± 0.5 · ParkTreeScaleVariation`) → **Transform Points** (giro 0 – 360°) → *Static Mesh Spawner*
   (`SM_Tree` con peso 2 y `SM_Bush` con peso 1).

## PCG_City

- *Graph Parameters*: `CitySize`, `BlockSize`, `BuildingDensity`, `BuildingHeight`, `BuildingVariation`,
  `ParkRatio`, `ParkTreeDensity` y `ParkTreeScaleVariation` (Double; el panel los escribe), y `GraphVersion`
  (la versión del código que montó los grafos).
- Un *Get Graph Parameter* por parámetro y un nodo *Subgraph* por sistema. Cada nodo *Subgraph* recibe por
  sus pines de parámetro los *Graph Parameters* que tiene su subgrafo (lista `Systems` en `BuildGraphs`).

## Añadir un tipo de mobiliario

1. En `BuildGraphs`, copia el bloque de `SG_Trees` con otro nombre (por ejemplo, `SG_Benches`) y otra malla.
2. Elige un `Phase` libre (0.25 o 0.75) para que no coincida con las farolas ni con los árboles.
3. Añade el subgrafo a `Systems` (con los parámetros que recibe) en el bloque de `PCG_City` y a `SubgraphNames`.
4. Sube `FCityContent::GraphVersion`. En la siguiente generación, los grafos se rehacen solos.
