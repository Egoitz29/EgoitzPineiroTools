# EgoitzPineiroTools · Generador procedural de ciudades

Plugin de Unreal Engine 5.8 que genera ciudades pequeñas con el PCG Framework: red de calles, manzanas
con acera, edificios, farolas y árboles. Los ingenieros de escenarios lo manejan desde un panel propio del
editor, sin abrir ningún grafo. Con la misma seed y los mismos parámetros sale siempre la misma ciudad.

## Instalación

1. Copia la carpeta `EgoitzPineiroTools` en `<TuProyecto>/Plugins/`. El proyecto tiene que ser de C++.
2. Haz clic derecho en el `.uproject` y elige *Generate Visual Studio project files*.
3. Compila el proyecto en *Development Editor* y ábrelo.
4. En *Edit › Plugins*, comprueba que **EgoitzPineiroTools - City Generator** está activado.

El plugin **PCG** se activa solo, porque está declarado como dependencia en el `.uplugin`. No hay que tocar
nada del proyecto anfitrión: el código, los grafos, los materiales y las mallas están dentro del plugin.

## Uso

1. Abre el panel con el botón **Ciudad** de la barra de herramientas (junto a *Play*) o desde
   *Tools › Generador de ciudades*.
2. Ajusta los parámetros y pulsa **Generar ciudad**. La ciudad aparece centrada en el origen del nivel,
   dentro de un actor `ProceduralCity`.
   - La primera vez, el botón crea también el contenido del plugin en `Content/CityGenerator`
     (materiales, mallas y grafos). No hace falta ningún paso manual.
3. **Limpiar ciudad** elimina por completo la ciudad: el actor y todo lo que PCG generó dentro de él.
   *Generar ciudad* también limpia la ciudad anterior antes de crear la nueva.
4. Con **Regenerar al cambiar un valor** activado, la ciudad se actualiza al soltar un deslizador o al
   escribir un valor.
5. **Aleatoria** elige una seed nueva al azar.

Bajo los botones aparecen los mensajes (avisos y errores), que se pueden seleccionar y copiar con
**Copiar mensajes**. La sección **Ciudad en el nivel** cuenta las instancias de cada tipo
(malla / material) y muestra una **huella**: un número que resume la posición, el giro y la escala de
todas las instancias. Dos ciudades con la misma huella son la misma ciudad.

**Reproducir un escenario:** basta con guardar la seed y los cinco parámetros.

## Parámetros

| Panel | Graph Parameter | Rango | Qué controla |
| --- | --- | --- | --- |
| Tamaño de la ciudad (m) | `CitySize` | 50 – 3000 | Lado de la ciudad cuadrada. Manzanas por lado = Floor((CitySize − 8) / (BlockSize + 8)). |
| Tamaño de manzana (m) | `BlockSize` | 15 – 300 | Lado de cada manzana, acera incluida. Con manzanas pequeñas hay más cruces. El panel muestra cuántas manzanas y cruces salen. |
| Densidad (0-1) | `BuildingDensity` | 0 – 1 | Parte de las parcelas que se construyen: 0 es una zona abierta y 1, un cañón urbano. |
| Altura media (m) | `BuildingHeight` | 3 – 300 | Altura media de los edificios. |
| Variación (0-1) | `BuildingVariation` | 0 – 1 | Diferencia entre edificios: altura ±75 %, huella entre el 85 % y el 55 % de la parcela y desplazamiento dentro de ella. Con 0, todos iguales. |
| Seed | *Seed* del PCG Component | 0 – 9 999 999 | Identifica el escenario. Cambia qué parcelas se construyen, la forma, la posición, el giro y el color de los edificios, y el giro y el tamaño de los árboles. |

Constantes, en las propiedades de cada nodo (en metros): calle 8, acera 3, bordillo 0.15, fondo de
parcela 18, frente de parcela ≈ 15, farolas cada 24 y árboles cada 24 (desfasados 12), a 6 de las esquinas.

## Cómo funciona

```
PCG_City ── pasa los parámetros a cada subgrafo ──┬─► SG_Streets      calzada
                                                   ├─► SG_Blocks       manzanas (acera)
                                                   ├─► SG_Buildings    edificios
                                                   ├─► SG_StreetLamps  farolas
                                                   └─► SG_Trees        árboles

Cada subgrafo empieza igual:  CitySize, BlockSize ─► City Street Grid ─► Roads / Blocks

SG_Buildings:  Blocks ─► City Lots ─► Density Filter ─► City Buildings ─► Transform Points ─► Self Pruning ─► Static Mesh Spawner
SG_Trees:      Blocks ─► City Sidewalk Points ─► Transform Points ─► Static Mesh Spawner
```

- **Calles y manzanas.** *City Street Grid* calcula una retícula de cruces centrada en el origen
  (`FCityGrid`). Saca dos conjuntos de puntos: `Roads` (un cruce en cada nudo y una calle entre cada dos
  cruces, sin solapes) y `Blocks` (el hueco entre cuatro cruces). Las manzanas salen de las calles, así que
  nunca las invaden.
- **Edificios.** *City Lots* reparte el borde interior de cada manzana (dentro de la acera) en parcelas
  que dan a la calle, y le pone a cada una un `$Density` con la seed. Un *Density Filter* estándar
  (0.5 – 1) deja pasar el porcentaje `BuildingDensity`. *City Buildings* calcula la altura, la huella y la
  posición de cada edificio dentro de su parcela. *Transform Points* le da unos grados de giro,
  *Self Pruning* elimina cualquier solape y *Static Mesh Spawner* elige uno de tres materiales.
- **Mobiliario.** *City Sidewalk Points* pone puntos a lo largo de las aceras, mirando a la calle y a
  6 m de las esquinas, para que nunca caigan en un cruce. Las farolas van cerca del bordillo y los
  árboles, a medio camino entre farola y farola (`Phase` 0.5), con giro y tamaño aleatorios.
- **Integración con el panel.** El panel escribe los valores en los *Graph Parameters* de `PCG_City`
  (`SetGraphParameter`) y la seed en el PCG Component, y llama a `GenerateLocal`. `PCG_City` pasa los
  parámetros a cada subgrafo por los pines de parámetro del nodo *Subgraph*.

Cada sistema está en su propio subgrafo y solo depende de los parámetros: añadir uno nuevo no cambia
los demás. El detalle nodo a nodo está en [`Docs/Grafos.md`](Docs/Grafos.md).

### Determinismo

Fuera de PCG no hay azar. Cada punto tiene un `$Seed` calculado a partir de su posición, y todos los nodos
aleatorios (*City Lots*, *City Buildings*, *Transform Points*, *Self Pruning* y *Static Mesh Spawner*) lo
combinan con la seed del PCG Component. Con la misma seed, cada parcela saca siempre los mismos números; con
otra seed, las reglas son las mismas pero los valores cambian.

### Limpieza

Todo lo que genera la herramienta vive dentro de un actor con el tag `EPT_CityGenerated`. Limpiar llama a
`CleanupLocal` en su PCG Component y después destruye el actor.

### Contenido generado por código

`Content/CityGenerator` no se edita a mano. Lo crea `FCityContent` la primera vez que se genera una ciudad:

- Materiales (`M_CityColor` y sus instancias) y mallas (`SM_StreetLamp`, `SM_Tree`): se crean si faltan.
- Grafos (`PCG_City` y `Subgraphs/SG_*`): llevan la versión del código que los montó, en el parámetro
  `GraphVersion` de `PCG_City`. Si falta un grafo o la versión no coincide, se vuelven a montar todos.
  **Rehacer grafos**, en el panel, los vuelve a montar a la fuerza.

## Estructura del plugin

```
EgoitzPineiroTools/
├── EgoitzPineiroTools.uplugin
├── Resources/Icon128.png
├── Docs/Grafos.md                          los grafos nodo a nodo
└── Source/
    ├── EgoitzPineiroToolsRuntime/          Runtime: nodos PCG y actor de la ciudad
    │   ├── Public/CityGrid.h               la retícula de calles (única fuente de verdad)
    │   ├── Public/ProceduralCityActor.h    el actor que contiene la ciudad
    │   └── Public/CityPCG/                 un nodo por archivo y los nombres de pines y atributos
    └── EgoitzPineiroToolsEditor/           Editor: panel, menú, botón y contenido
        └── Private/
            ├── SCityPanel                  la interfaz Slate
            ├── CityGenerator               generar, limpiar y estadísticas (sin interfaz)
            ├── CityContent                 crea los materiales, las mallas y los grafos
            └── CityParams.h                los seis parámetros y sus nombres en los grafos
```

Los nodos están en un módulo **Runtime** para que existan también en una build empaquetada. El panel está en
un módulo **Editor**, que nunca llega a la build final.

## Ampliar

- **Un tipo de mobiliario nuevo** (bancos, papeleras…): copia la parte de `SG_Trees` en
  `CityContent.cpp › BuildGraphs`. Usa otro `Phase` (por ejemplo, 0.25) para que no coincida con farolas ni
  árboles, añade el subgrafo a la lista `Systems` de `PCG_City` y sube `FCityContent::GraphVersion`. Los
  sistemas que ya existen no cambian.
- **Otros edificios:** añade entradas al *Static Mesh Spawner* de `SG_Buildings`.
- **Otra capa:** escribe un nodo nuevo partiendo de cualquiera de `Source/EgoitzPineiroToolsRuntime/Public/CityPCG`.
  Las utilidades de `CityPCGUtils.h` leen y crean puntos con el mismo convenio de cajas.

Cuando cambies cómo se montan los grafos, sube `FCityContent::GraphVersion`: la siguiente generación los
rehará sola en todos los proyectos.

## Demostración

1. **Generación completa:** pulsa *Generar ciudad* con los valores por defecto.
2. **Misma seed, mismo resultado:** anota la huella, pulsa *Limpiar ciudad* y luego *Generar ciudad*. Sale la
   misma huella.
3. **Otra seed, otra ciudad:** pulsa *Aleatoria* y *Generar ciudad*. Cambian los edificios, los colores y los
   árboles, y también la huella; las reglas son las mismas.
4. **Efecto de los parámetros:** activa *Regenerar al cambiar un valor* y mueve cada deslizador.

## Limitaciones conocidas

- La ciudad es una retícula regular, centrada en el origen del mundo. Mover el actor no mueve la ciudad.
- La generación se hace en el editor; la ciudad generada se guarda con el nivel.
