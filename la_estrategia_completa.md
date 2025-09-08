Análisis y Flujo de Trabajo para la Implementación de Estrategia de Trading (Modo Scalping)

Este documento sirve como la especificación técnica para evolucionar el Asesor Experto (EA) actual, integrando la lógica de trading descrita.

1. Resumen del Objetivo

El objetivo es implementar una estrategia de trading "Scalping" que opere dentro de las zonas definidas por los niveles de Fibonacci del período anterior. El EA deberá gestionar la apertura, el seguimiento y el cierre de operaciones basándose en un conjunto de reglas que incluyen: un horario operativo específico, una gestión de riesgo basada en grilla (grid) y Martingala, y un objetivo de ganancia diaria.

2. Desglose Funcional por Módulos

Para una implementación limpia y mantenible, dividiremos la lógica en los siguientes módulos:

Módulo 1: Control de Sesión (Horario y Días)

Este módulo actuará como el primer filtro. Decidirá si el bot tiene permiso para operar en el momento actual.

    Funcionalidad:

        Verificar si el día actual de la semana está habilitado para operar.

        Verificar si la hora actual (en UTC) se encuentra dentro del rango horario definido.

        Gestionar un estado global: MODO_ACTIVO (abriendo y gestionando trades) o MODO_CIERRE_SOLAMENTE (solo gestionando trades abiertos hasta que se cierren todos).

    Parámetros de Entrada (input) Requeridos:

        bool _EnableDayFilter: Habilitar/deshabilitar filtro de días.

        bool _TradeOnMonday: Operar Lunes.

        bool _TradeOnTuesday: Operar Martes.

        bool _TradeOnWednesday: Operar Miércoles.

        bool _TradeOnThursday: Operar Jueves.

        bool _TradeOnFriday: Operar Viernes.

        bool _EnableTimeFilter: Habilitar/deshabilitar filtro de horas.

        int  _StartTimeUTC: Hora de inicio (ej. 8 para 08:00 UTC).

        int  _EndTimeUTC: Hora de fin (ej. 11 para 11:00 UTC).

Módulo 2: Lógica de Entrada y Dirección del Trade

Este módulo utilizará las zonas ya calculadas por el código existente (g_CurrentZone) para decidir qué tipo de operaciones abrir.

    Funcionalidad:

        Basado en g_CurrentZone, determinar la dirección permitida:

            ZONE_BUY: Solo compras.

            ZONE_SELL: Solo ventas.

            ZONE_RANGING: Compras y ventas simultáneamente.

            ZONE_OUT_OF_RANGE: No abrir ninguna operación nueva.

        Al inicio de la ventana operativa (o al entrar en una zona válida dentro de ella), si no hay operaciones abiertas de un tipo específico (compra/venta), abrir la operación inicial a precio de mercado.

    Parámetros de Entrada (input) Requeridos:

        double _InitialLotSize: Lotaje inicial (ej. 0.01).

Módulo 3: Gestión de Órdenes y Riesgo (El Núcleo de la Estrategia)

Este es el módulo más complejo. Se encargará de la gestión de cada "canasta" de operaciones (todas las compras son una canasta, todas las ventas son otra).

    Funcionalidad:

        Cálculo del Take Profit (TP): La estrategia define un TP en dólares. Esto debe ser convertido a un precio absoluto. La fórmula es:

            Para una compra: TP_precio=Precio_apertura+TP_pips

            Para una venta: TP_precio=Precio_apertura−TP_pips

            Donde TP_pips se calcula como: $TP\_{pips} = \\frac{TP\_{$}}{ValorPipPorLote \times Lotaje}$

            El ValorPipPorLote se obtiene de las propiedades del símbolo. El bot debe calcular esto dinámicamente.

        Re-apertura en Ganancia: Cuando una operación (o un grupo de ellas) cierra en ganancia, si el bot sigue en MODO_ACTIVO, debe abrir inmediatamente una nueva operación inicial en la misma dirección.

        Lógica de Grid (Pip Step):

            Para cada canasta (compras/ventas), monitorear el precio actual.

            Si el precio se mueve en contra de la última operación una distancia igual al _PipStep, abrir una nueva operación en la misma dirección.

            El _PipStep se define en dólares de movimiento del precio. Esto es más robusto que pips, pero requiere una conversión. $Distancia\_{puntos} = \\frac{PipStep\_{$}}{symbol_info.Point()}$. Por ejemplo, si un movimiento de $5 en el precio del activo (ej. un índice de 3000 a 2995) es el _PipStep, el bot debe calcular la diferencia en puntos.

        Lógica de Martingala:

            La primera operación de la grilla (la segunda en total) se abre con el mismo lotaje (_InitialLotSize).

            A partir de la tercera operación en la misma canasta, el lotaje se multiplica por el factor Martingala.

            Ejemplo: 0.01 (inicial), 0.01 (grid 1), 0.02 (grid 2, _MartingaleMultiplier=2.0), 0.04 (grid 3), etc.

        Ajuste Dinámico del Take Profit (Promedio):

            Cada vez que se añade una operación a una canasta (ej. una nueva compra), el TP de todas las operaciones de esa canasta debe ser recalculado y modificado.

            El nuevo objetivo es cerrar todas las operaciones de la canasta juntas en un punto que cubra los costos y genere la ganancia deseada (_TakeProfitDollars).

            Paso 1: Calcular el precio promedio ponderado de la canasta:
            PrecioPromedio=∑i=1n​Lotajei​∑i=1n​(PrecioAperturai​×Lotajei​)​

            Paso 2: Calcular el nuevo precio de TP:

                Para compras: NuevoTP_precio=PrecioPromedio+textGananciaRequeridaEnPips

                Para ventas: NuevoTP_precio=PrecioPromedio−textGananciaRequeridaEnPips

                La GananciaRequeridaEnPips se calcula similar a antes, pero usando el lotaje total de la canasta.

    Parámetros de Entrada (input) Requeridos:

        ENUM_TP_MODE _TP_Mode: Elección entre IN_DOLLARS o IN_PIPS.

        double _TakeProfitValue: Valor del TP (ej. 0.5 para dólares o 50 para pips).

        double _PipStepValue: Distancia en dólares del precio para la siguiente operación de grid (ej. 5.0).

        bool   _EnableMartingale: Habilitar/deshabilitar Martingala después del primer nivel de grid.

        double _MartingaleMultiplier: Factor de multiplicación (ej. 1.5 para suave, 2.0 para agresivo).

        int    _MartingaleStartLevel: Nivel de grid en el que empieza la martingala (ej. 2 para empezar en la 3ra orden).

Módulo 4: Criterios de Finalización de Jornada

Este módulo decide cuándo pasar del MODO_ACTIVO al MODO_CIERRE_SOLAMENTE.

    Funcionalidad:

        Objetivo de Ganancia Diaria:

            Calcular continuamente la ganancia total del día para este EA (usando el _MagicNumber).

            Calcular el % de ganancia sobre el balance de la cuenta al inicio del día.

            Si GananciaDiaria_% >= _DailyProfitTarget_%, cambiar el estado a MODO_CIERRE_SOLAMENTE. No se abrirán más operaciones nuevas, ni siquiera las de re-apertura.

        Fin de Horario Operativo:

            Si la hora actual supera _EndTimeUTC, cambiar el estado a MODO_CIERRE_SOLAMENTE.

    Parámetros de Entrada (input) Requeridos:

        double _DailyProfitTarget: Porcentaje de ganancia diaria objetivo (ej. 1.0 para 1%).

Módulo 5: Espacio Reservado para "Modo Recovery"

    Funcionalidad:

        Se dejará una sección comentada en el código, principalmente en el OnTick(), y un parámetro de entrada para activar este modo en el futuro.

        // --- INICIO: SECCIÓN RESERVADA PARA MODO RECOVERY ---

        // --- FIN: SECCIÓN RESERVADA PARA MODO RECOVERY ---

    Parámetros de Entrada (input) Requeridos:

        bool _EnableRecoveryMode: (Por ahora deshabilitado por defecto) Activar Modo Recovery.

3. Arquitectura y Flujo Lógico en el Código

La implementación se centrará en el OnTick() y en nuevas funciones de ayuda.

Nuevas Variables Globales:
C++

// --- ESTADOS Y GESTIÓN DE SESIÓN ---
bool g_is_trading_active = false;      // ¿Estamos dentro de la ventana de tiempo y por debajo del profit diario?
bool g_daily_goal_reached = false;     // ¿Se alcanzó el objetivo diario?
datetime g_trading_day_start = 0;      // Para rastrear el inicio del día de trading
double g_balance_at_start_of_day = 0;  // Balance para calcular el % de ganancia

Flujo en OnTick():
C++

void OnTick()
{
    // 1. Actualiza el Fibo base (se mantiene igual, solo se ejecuta en un nuevo periodo)
    UpdateBasePeriodAnalysis();

    // 2. Detecta y dibuja las zonas (se mantiene igual)
    DetectAndDrawZones();

    // ==================================================================
    //                  NUEVO FLUJO DE TRADING
    // ==================================================================

    // 3. Chequeo de fin de día/semana para resetear contadores
    CheckForDayReset(); // Resetea g_daily_goal_reached y g_balance_at_start_of_day

    // 4. Módulo 1: ¿Tenemos permiso para operar?
    // Si el objetivo diario ya se cumplió, no hacemos nada más que gestionar trades existentes.
    if(g_daily_goal_reached)
    {
        ManageExistingTrades(); // Podría ser parte de los siguientes pasos
        return; 
    }
    
    // Verificar si estamos dentro de la ventana de días y horas permitidas.
    if(!IsWithinTradingWindow())
    {
        // Si hay trades abiertos, debemos pasar a modo "Cierre Solamente"
        // No abrimos nuevos trades.
        ManageExistingTrades();
        return;
    }

    // 5. Módulo 4: Verificar si alcanzamos el objetivo de ganancia ahora mismo
    if(CheckDailyProfitTarget())
    {
        g_daily_goal_reached = true;
        Print("Objetivo de ganancia diaria alcanzado. Pasando a modo 'Cierre Solamente'.");
        ManageExistingTrades();
        return;
    }
    
    // Si llegamos aquí, estamos en MODO_ACTIVO.

    // 6. Módulo 2 y 3: Ejecutar la lógica de trading según la zona
    switch(g_CurrentZone)
    {
        case ZONE_BUY:
            ManageSellTrades(true); // El 'true' indica cerrar todo si es posible
            ManageBuyTrades(false); // El 'false' indica operativa normal (grid/martingala)
            break;
            
        case ZONE_SELL:
            ManageBuyTrades(true);  // Cerrar compras
            ManageSellTrades(false);// Operativa normal de ventas
            break;

        case ZONE_RANGING:
            ManageBuyTrades(false); // Operativa normal de compras
            ManageSellTrades(false);// Operativa normal de ventas
            break;

        case ZONE_OUT_OF_RANGE:
            Print("Fuera de rango. No se abren nuevas operaciones.");
            ManageExistingTrades(); // Gestionar lo que ya esté abierto
            break;
    }
}

Nuevas Funciones a Crear:

    bool IsWithinTradingWindow(): Implementa la lógica del Módulo 1.

    void CheckForDayReset(): Resetea las variables de sesión diariamente.

    bool CheckDailyProfitTarget(): Implementa la lógica del Módulo 4.

    void ManageBuyTrades(bool close_all): Gestiona la canasta de compras (apertura inicial, grid, martingala, re-cálculo de TP). Si close_all es true, busca cerrar en lugar de expandir la grilla.

    void ManageSellTrades(bool close_all): Idem para la canasta de ventas.

    void CalculateAndModifyTP(ENUM_ORDER_TYPE direction): Función que calcula el precio promedio ponderado y modifica el TP de todas las órdenes de una dirección.

    double GetPipValue(): Función para obtener el valor del pip en la moneda de la cuenta para el lotaje actual.

4. Puntos Críticos y Recomendaciones

    Riesgo de Martingala: Es fundamental que el usuario final comprenda que la Martingala es una estrategia de altísimo riesgo que puede llevar a la pérdida total de la cuenta. Los parámetros _MartingaleMultiplier y _PipStepValue deben ser ajustados con extrema precaución.

    Gestión de Estado: La transición entre MODO_ACTIVO y MODO_CIERRE_SOLAMENTE es crítica. El bot no debe abrir NINGUNA operación nueva (ni por grid, ni por re-apertura) una vez que se cumple una condición de salida.

    Eficiencia: La gestión de órdenes no debe ocurrir en cada tick. Se debe implementar una salvaguarda para que estas lógicas complejas se ejecuten, por ejemplo, solo una vez por segundo o en cada nueva barra del timeframe operativo (ej. M1), para no sobrecargar el terminal.

    Cálculo de TP: La conversión de dólares a pips/puntos es dependiente del activo. Debe ser robusta y manejar correctamente diferentes tipos de símbolos (Forex, Índices, Cripto). La función SymbolInfoDouble(symbol, SYMBOL_TRADE_TICK_VALUE) es clave aquí.

5. Próximos Pasos

    Validación: Revisar y validar este flujo de trabajo.

    Implementación por Módulos: Proceder a la codificación, comenzando por los parámetros de entrada (input) y el Módulo 1 (Control de Sesión), que es la base de todo.

    Desarrollo del Core: Implementar el Módulo 3 (Gestión de Órdenes), que es el más complejo, prestando especial atención a los cálculos matemáticos.

    Pruebas en Backtesting: Realizar pruebas exhaustivas en el Probador de Estrategias de MetaTrader 5, probando diferentes configuraciones de riesgo.

    Pruebas en Demo: Antes de usar en una cuenta real, es imperativo probar el EA en una cuenta demo durante un tiempo considerable.