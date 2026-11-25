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
│   └── SG_Trees             árboles
├── Materials/               M_CityColor y sus instancias MI_*
└── Meshes/                  SM_StreetLamp, SM_Tree
```

## Convenio de puntos

Calles, manzanas, parcelas y edificios son puntos que se leen como cajas: el transform lleva la posición, el
giro y el tamaño (como escala), y las bounds son un cubo de 1 m. Así, un *Static Mesh Spawner* con
`/Engine/BasicShapes/Cube` dibuja cada punto a su tamaño real, y *Self Pruning* ve las cajas reales. Los
puntos de mobiliario tienen escala 1, para que las mallas conserven su tamaño.

## Nodos C++ propios

| Nodo | Entradas | Salidas | Atributos | Parámetros conectados |
| --- | --- | --- | --- | --- |
| City Street Grid | — | `Roads`, `Blocks` | Roads: `RoadKind`, `Axis`, `Col`, `Row`. Blocks: `BlockID`, `Col`, `Row` | `CitySize`, `BlockSize` |
| City Lots | `Blocks` | `Lots` | `BlockID`, `Side`, `$Density` | `BuildingDensity` |
| City Buildings | `Lots` | `Buildings` | `Height`, `Floors` | `BuildingHeight`, `BuildingVariation` |
| City Sidewalk Points | `Blocks` | `Points` | `BlockID`, `Side` | — |

Los parámetros se conectan a las propiedades del mismo nombre (pines de override de PCG). Las demás
propiedades de cada nodo son constantes que se pueden cambiar en *Details*.

## Subgrafos

Todos los subgrafos empiezan igual: dos *Get Graph Parameter* (`CitySize`, `BlockSize`) conectados a un
*City Street Grid*. Cada subgrafo vuelve a calcular la retícula a partir de los parámetros: no recibe datos
de otros subgrafos, así que se puede quitar o añadir uno sin tocar los demás.

### SG_Streets
`City Street Grid.Roads` → *Static Mesh Spawner* (Cube, `MI_Road`).

### SG_Blocks
`City Street Grid.Blocks` → *Static Mesh Spawner* (Cube, `MI_Sidewalk`). Cada manzana es una losa de
15 cm sobre la calzada: la acera.

### SG_Buildings
Parámetros extra: `BuildingDensity`, `BuildingHeight` y `BuildingVariation`.

1. `City Street Grid.Blocks` → **City Lots**. Dentro del anillo de acera (3 m), una franja de 18 m de fondo
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

## PCG_City

- *Graph Parameters*: `CitySize`, `BlockSize`, `BuildingDensity`, `BuildingHeight`, `BuildingVariation`
  (Double; el panel los escribe) y `GraphVersion` (la versión del código que montó los grafos).
- Un *Get Graph Parameter* por parámetro y un nodo *Subgraph* por sistema. Los pines de parámetro de cada
  nodo *Subgraph* reciben `CitySize` y `BlockSize`, y el de `SG_Buildings` recibe también los tres de los
  edificios.

## Añadir un tipo de mobiliario

1. En `BuildGraphs`, copia el bloque de `SG_Trees` con otro nombre (por ejemplo, `SG_Benches`) y otra malla.
2. Elige un `Phase` libre (0.25 o 0.75) para que no coincida con las farolas ni con los árboles.
3. Añade el subgrafo a `Systems` en el bloque de `PCG_City` y a `SubgraphNames`.
4. Sube `FCityContent::GraphVersion`. En la siguiente generación, los grafos se rehacen solos.
