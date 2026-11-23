# Grafos PCG de la ciudad

Este documento describe, nodo a nodo, los grafos que hay en `Content/City`. Sirve para entender qué
hace cada uno antes de ampliarlo, o para montarlos a mano.

**Todo este contenido lo crea el botón *Crear contenido* del panel** (código en
`Source/EgoitzPineiroTools/Private/CityContentBuilder.cpp`). El botón solo crea lo que falta: para
rehacer un asset, bórralo en el Content Browser y vuelve a pulsarlo. Si algo no encaja con la versión
del motor, el panel muestra el error con el nombre de la propiedad o del pin que falla.

La lógica que depende de bucles y de geometría (rejilla, tramos, manzanas, parcelas, forma de los
edificios y huecos de acera) está en los nodos C++ del módulo `EgoitzPineiroToolsRuntime`. Las
decisiones (qué parcelas se construyen, variación aleatoria, poda de solapes) y la generación de
mallas usan nodos estándar de PCG.

## Contenido

```
Content/City/
├── PCG_City                      grafo principal: solo cablea los sistemas
├── Subgraphs/
│   ├── SG_Streets                cruces y tramos de calle
│   ├── SG_Blocks                 manzanas (acera) y huecos de mobiliario
│   ├── SG_Buildings              parcelas, densidad y edificios
│   ├── SG_StreetLamps            farolas
│   └── SG_Trees                  árboles
├── Materials/
│   ├── M_CityColor               material base con un parámetro Color
│   └── MI_Road, MI_Sidewalk, MI_Building_A/B/C, MI_LampPost, MI_LampLight, MI_TreeTrunk, MI_TreeLeaves
└── Meshes/
    ├── SM_StreetLamp
    └── SM_Tree
```

> **La ruta del grafo principal es fija:** la herramienta carga `/EgoitzPineiroTools/City/PCG_City`.
> Si lo mueves o lo renombras, el panel avisa de que no lo encuentra.

## Antes de empezar

- **Content Browser › Settings:** activa *Show Plugin Content* (para ver `EgoitzPineiroTools Content`) y
  *Show Engine Content* (para elegir `/Engine/BasicShapes/Cube`).
- **Graph Parameters:** panel *Graph Parameters* del editor de grafos › **+** › nombre y tipo. Todos los
  de este documento son **Double**, con el nombre exacto (mayúsculas incluidas).
- **Pines de override:** arrastra el parámetro al grafo (aparece un nodo *Get …*) y conéctalo al pin con
  el mismo nombre del nodo. Si el pin no se ve, despliega los pines avanzados del nodo (flecha de la
  parte inferior).
- **Pines propios de un subgrafo:** selecciona el nodo *Input* u *Output* › Details › *Custom Pins* › **+**.
  Pon el *Label* y el tipo **Point**.
- **Static Mesh Spawner:** *Mesh Selector Type* = *Weighted* › *Mesh Entries* › **+** › *Descriptor* ›
  *Static Mesh* y, si hace falta, *Override Materials*.
- **Depurar:** `D` (Debug) dibuja los puntos de un nodo; `A` (Inspect) muestra su tabla de atributos.

## 1. Materiales

1. `Materials` › clic derecho › *Material* › `M_CityColor`.
2. Dentro: un *VectorParameter* llamado `Color` a *Base Color* y un *ScalarParameter* llamado
   `Roughness` (valor 0.8) a *Roughness*. Guarda.
3. Clic derecho sobre `M_CityColor` › *Create Material Instance*, una por fila:

| Instancia | Color (R, G, B) |
| --- | --- |
| `MI_Road` | 0.03, 0.03, 0.035 |
| `MI_Sidewalk` | 0.45, 0.45, 0.42 |
| `MI_Building_A` | 0.70, 0.62, 0.50 |
| `MI_Building_B` | 0.36, 0.42, 0.52 |
| `MI_Building_C` | 0.55, 0.30, 0.24 |
| `MI_LampPost` | 0.08, 0.08, 0.08 |
| `MI_LampLight` | 1.00, 0.85, 0.45 |
| `MI_TreeTrunk` | 0.25, 0.15, 0.08 |
| `MI_TreeLeaves` | 0.10, 0.35, 0.10 |

## 2. Mallas de mobiliario

El botón construye las dos mallas por código: un cilindro y un elipsoide cada una, con el **pivote en la
base** (Z = 0), porque los puntos de acera están a la altura de la acera y la malla se apoya en ellos.
A mano se pueden hacer igual con formas básicas y *Merge Actors*:

**`SM_StreetLamp`**

1. En un nivel vacío de trabajo (no lo guardes), *Place Actors › Shapes*:
   - *Cylinder* en (0, 0, 225), escala (0.12, 0.12, 4.5), material `MI_LampPost`.
   - *Sphere* en (0, 0, 460), escala (0.5, 0.5, 0.35), material `MI_LampLight`.
2. Selecciona los dos › *Tools › Merge Actors* › activa **Pivot Point At Zero** › *Merge Actors*.
3. Guárdalo en `Content/City/Meshes` como `SM_StreetLamp`.

**`SM_Tree`**

1. *Cylinder* en (0, 0, 100), escala (0.25, 0.25, 2.0), material `MI_TreeTrunk`.
2. *Sphere* en (0, 0, 330), escala (2.6, 2.6, 2.8), material `MI_TreeLeaves`.
3. *Merge Actors* con **Pivot Point At Zero** › `Content/City/Meshes/SM_Tree`.

Borra los actores de trabajo. Cualquier malla con el pivote en la base sirve en su lugar (por ejemplo,
assets gratuitos), siempre que esté dentro del Content del plugin.

## 3. `SG_Streets`: la red de calles

*Graph Parameters:* `CitySize` (400), `BlockSize` (60). *Output:* pin `Crossings`.

| # | Nodo | Entradas | Ajustes |
| --- | --- | --- | --- |
| 1 | City Crossings | `CitySize` ← Get CitySize · `BlockSize` ← Get BlockSize | StreetWidth 8, RoadThickness 0.05 |
| 2 | City Street Segments | `Crossings` ← 1 | — |
| 3 | Merge Points | `In` ← 1 y 2 | — |
| 4 | Static Mesh Spawner | `In` ← 3 | Cube + `MI_Road` |
| — | Output | `Crossings` ← 1 | — |

**Prueba:** un PCG Volume con `SG_Streets` › *Generate*: una rejilla de losas oscuras de 8 m de ancho.

## 4. `SG_Blocks`: manzanas y aceras

*Input:* pin `Crossings`. *Output:* pines `Blocks` y `SidewalkSlots`.

| # | Nodo | Entradas | Ajustes |
| --- | --- | --- | --- |
| 1 | City Blocks | `Crossings` ← Input | SidewalkHeight 0.15 |
| 2 | Static Mesh Spawner | `In` ← 1 | Cube + `MI_Sidewalk` |
| 3 | City Sidewalk Slots | `Blocks` ← 1 · `Crossings` ← Input | SidewalkWidth 3, PropSpacing 12, CornerClearance 5, NumPropTypes 2 |
| — | Output | `Blocks` ← 1 · `SidewalkSlots` ← 3 | — |

Cada manzana es una losa gris de 15 cm que rellena exactamente el hueco entre calles: toda la manzana
está a nivel de acera, y los edificios se levantan encima.

## 5. `SG_Buildings`: parcelas, densidad y edificios

*Graph Parameters:* `BuildingDensity` (0.6), `BuildingHeight` (30), `BuildingVariation` (0.5).
*Input:* pin `Blocks`. *Output:* pin `Buildings`.

| # | Nodo | Entradas | Ajustes |
| --- | --- | --- | --- |
| 1 | City Lots | `Blocks` ← Input | SidewalkWidth 3, LotSize 20 |
| 2 | Attribute Filter | `In` ← 1 | Target Attribute `Frontage`, Operator Equal, constante Bool = true. Sigue por **In Filter** |
| 3 | Attribute Noise | `In` ← 2 (In Filter) | Input Source `$Density`, Mode Set, Noise Min 0, Noise Max 1 |
| 4 | Density Filter | `In` ← 3 · `Upper Bound` ← Get BuildingDensity | Lower Bound 0 |
| 5 | City Buildings | `Lots` ← 4 · `BuildingHeight` ← Get · `BuildingVariation` ← Get | MinHeight 3, MaxFootprint 0.9, FootprintVariation 0.3 |
| 6 | Transform Points | `In` ← 5 | Offset Min (-50, -50, 0), Offset Max (50, 50, 0) |
| 7 | Self Pruning | `In` ← 6 | Pruning Type: Large to Small |
| 8 | Static Mesh Spawner | `In` ← 7 | 3 entradas: Cube + `MI_Building_A`, Cube + `MI_Building_B`, Cube + `MI_Building_C`, peso 1 |
| — | Output | `Buildings` ← 7 | — |

Qué hace cada paso:

- **2 · Frontage:** solo se construyen las parcelas que dan a la acera; las interiores quedan como patio.
- **3 + 4 · Densidad:** cada parcela recibe un `$Density` aleatorio de 0 a 1 a partir de su `$Seed` y de
  la seed del componente, y pasa si es menor que `BuildingDensity`. Con 0 no hay edificios; con 1 se
  construyen todas las parcelas de fachada.
- **5 · Forma:** altura alrededor de `BuildingHeight` y huella entre el 60 % y el 90 % de la parcela,
  más dispersas cuanto mayor es `BuildingVariation`. Mira a la calle.
- **6 · Posición:** un desplazamiento de hasta ±0,5 m que depende de la seed. Con la huella como máximo
  al 90 % de la parcela, el edificio nunca llega a la acera.
- **7 · Solapes:** red de seguridad; por construcción no debería haber ninguno.

## 6. Mobiliario: `SG_StreetLamps` y `SG_Trees`

Los dos reciben los mismos puntos de acera (*Input:* pin `Slots`) y cada uno se queda con su tipo.

**`SG_StreetLamps`**

| # | Nodo | Entradas | Ajustes |
| --- | --- | --- | --- |
| 1 | Attribute Filter | `In` ← Input | Target Attribute `PropType`, Equal, constante Integer = 0 |
| 2 | Static Mesh Spawner | `In` ← 1 (In Filter) | `SM_StreetLamp` |

**`SG_Trees`**

| # | Nodo | Entradas | Ajustes |
| --- | --- | --- | --- |
| 1 | Attribute Filter | `In` ← Input | Target Attribute `PropType`, Equal, constante Integer = 1 |
| 2 | Transform Points | `In` ← 1 (In Filter) | Rotation Max Z = 360, Scale Min 0.8, Scale Max 1.3, Uniform Scale |
| 3 | Static Mesh Spawner | `In` ← 2 | `SM_Tree` |

Las farolas miran a la calle (el giro viene del punto); los árboles reciben un giro y un tamaño
aleatorios que dependen de la seed.

## 7. `PCG_City`: el grafo principal

*Graph Parameters* (todos Double): `CitySize` 400, `BlockSize` 60, `BuildingDensity` 0.6,
`BuildingHeight` 30, `BuildingVariation` 0.5. Son los que escribe el panel.

Arrastra los cinco subgrafos al grafo y conéctalos así:

```
Get CitySize ──┐
Get BlockSize ─┤
               ▼
          SG_Streets ──Crossings──► SG_Blocks ──Blocks─────────► SG_Buildings ◄── Get BuildingDensity
                                        │                                     ◄── Get BuildingHeight
                                        │                                     ◄── Get BuildingVariation
                                        └──SidewalkSlots──┬────► SG_StreetLamps (Slots)
                                                          └────► SG_Trees (Slots)
```

El grafo principal no calcula nada: solo pasa datos y parámetros de un sistema al siguiente.

> **Bug silencioso:** si un parámetro no llega a un subgrafo por cable, el subgrafo usa su valor por
> defecto y no avisa. Comprueba que cada *Get* del grafo principal está conectado a su pin.

**Prueba:** un PCG Volume con `PCG_City` › *Generate*. Con los valores por defecto deberías ver:

| Capa | Puntos |
| --- | --- |
| Cruces | 36 (6 × 6) |
| Tramos de calle | 60 |
| Manzanas | 25 |
| Parcelas / de fachada | 225 / 200 |
| Edificios | unos 120 (60 % de 200) |
| Huecos de acera | 400 (200 farolas y 200 árboles) |

Cambia el *Seed* del volumen: cambian los edificios y los árboles, no las calles. Vuelve al Seed
anterior: sale exactamente la misma ciudad. Cuando funcione, **borra el PCG Volume**: la herramienta
crea y destruye su propio actor.

## Ampliar el sistema

- **Un tipo de mobiliario nuevo** (por ejemplo, bancos): pon `NumPropTypes` = 3 en *City Sidewalk Slots*
  y crea `SG_Benches` igual que `SG_StreetLamps`, con `PropType == 2`. Conéctalo a `SidewalkSlots` en
  `PCG_City`. Los subgrafos que ya existen no cambian.
- **Otro aspecto de edificios:** añade entradas (malla y material) al Static Mesh Spawner de
  `SG_Buildings`. Si la malla no es un cubo de 1 m con el pivote en el centro, ajusta su escala.
- **Constantes:** ancho de calle (`StreetWidth`), de acera (`SidewalkWidth`, en *City Lots* y en
  *City Sidewalk Slots*, con el mismo valor), tamaño de parcela y separación del mobiliario están en
  Details de cada nodo.
