#property copyright "m0 / El ProfeXau"
#property link      ""

// --- INCLUDES NECESARIOS ---
#include <Trade\Trade.mqh>
#include <Trade\PositionInfo.mqh>
#include <Trade\DealInfo.mqh>

//+------------------------------------------------------------------+
//|             SECCIÓN 1: INPUTS (PARÁMETROS DEL USUARIO)           |
//+------------------------------------------------------------------+

// --- GESTIÓN DE RIESGO Y GANANCIAS ---
input group           "Gestión de Riesgo"
input double          _LotSize = 0.01;                        ///< [USD] Lotaje inicial para la primera operación.
input double          _TakeProfitInCurrency = 0.5;            ///< [USD] Take Profit deseado en la moneda de la cuenta (ej: 0.5 para 50 centavos).
input double          _DailyProfitGoalPercent = 1.0;          ///< [%] Meta de ganancia diaria como porcentaje del balance. El EA se detiene al alcanzarla.

// --- ESTRATEGIA DE MARTINGALA ---
input group           "Estrategia de Parrilla (Grid)"
input double          _PipStep = 500.0;                       ///< Distancia en PUNTOS para abrir la siguiente operación en la parrilla.
input double          _MartingaleMultiplier = 2.0;            ///< Multiplicador de lotaje para la siguiente operación (ej: 2.0 para duplicar).

// --- HORARIO DE OPERACIÓN (UTC) ---
input group           "Horario de Operación (UTC)"
input bool            _EnableTimeFilter = true;               ///< Habilitar/Deshabilitar el filtro de horario. Si es 'false', opera 24h.
input int             _StartHour = 8;                         ///< [UTC] Hora de inicio de la operativa (ej: 8 para 08:00).
input int             _EndHour = 11;                          ///< [UTC] Hora de fin para abrir NUEVAS operaciones (ej: 11 para 11:00).

// --- DÍAS DE OPERACIÓN ---
input group           "Días de Operación"
input bool            _EnableDayFilter = true;                ///< Habilitar/Deshabilitar el filtro de días. Si es 'false', opera todos los días.
input bool            _WorkOnMonday = true;                   ///< ¿Operar el Lunes?
input bool            _WorkOnTuesday = true;                  ///< ¿Operar el Martes?
input bool            _WorkOnWednesday = true;                ///< ¿Operar el Miércoles?
input bool            _WorkOnThursday = true;                 ///< ¿Operar el Jueves?
input bool            _WorkOnFriday = false;                  ///< ¿Operar el Viernes?

//+------------------------------------------------------------------+
//|               SECCIÓN 2: ENUMS Y VARIABLES GLOBALES              |
//+------------------------------------------------------------------+

/**
 * @brief Define los estados posibles del Asesor Experto.
 * Gestiona el comportamiento general del robot.
 */
enum ENUM_EA_STATE
{
    STATE_IDLE,                     ///< El EA está inactivo (fuera de horario/día o pausado).
    STATE_ANALYZING,                ///< El EA está en horario, pero esperando la zona correcta para operar.
    STATE_TRADING,                  ///< El EA está activamente buscando y gestionando operaciones.
    STATE_CLOSING_ONLY              ///< Meta de ganancias o fin de horario alcanzado. Solo gestiona cierres, no abre nada nuevo.
};

// --- Instancias de Clases ---
CTrade              trade;
CPositionInfo       posInfo;
CDealInfo           dealInfo;

// --- Variables de Estado y Control ---
ENUM_EA_STATE       g_CurrentState = STATE_IDLE;      // Guarda el estado actual de la máquina de estados.
int                 g_LastCheckDay = 0;               // Utilizado para resetear la meta de ganancias cada día.
bool                g_ProfitGoalReached = false;      // Bandera que se activa al alcanzar la meta de ganancias del día.

//+------------------------------------------------------------------+
//|          SECCIÓN 3: FUNCIÓN PRINCIPAL (ORQUESTADOR)              |
//+------------------------------------------------------------------+

/**
 * @brief Punto de entrada principal para la lógica de la estrategia.
 * Esta función es llamada en cada Tick desde el archivo principal.
 * @param zone La zona operativa actual calculada en el archivo principal.
 */
void ExecuteStrategy(ENUM_OPERATIVE_ZONE zone)
{
    UpdateEAState(); // Primero, actualizamos el estado general del EA (¿Puede operar ahora?)

    // Ejecutamos la lógica correspondiente al estado actual
    switch(g_CurrentState)
    {
        case STATE_TRADING:
            ManageTradesByZone(zone);
            break;

        case STATE_CLOSING_ONLY:
            // No hacemos nada activamente, solo dejamos que los Take Profits existentes actúen.
            // No se abren operaciones nuevas, ni de inicio ni de martingala.
            Print("Modo CIERRE: Esperando que las operaciones abiertas se cierren solas.");
            break;

        case STATE_IDLE:
        case STATE_ANALYZING:
            // No hacemos nada, estamos esperando las condiciones correctas.
            Print("Modo INACTIVO/ANÁLISIS: Esperando horario o zona de trading válida...");
            break;
    }
}

//+------------------------------------------------------------------+
//|          SECCIÓN 4: MÁQUINA DE ESTADOS Y FILTROS                 |
//+------------------------------------------------------------------+

/**
 * @brief Actualiza el estado global del EA (g_CurrentState) basado en los filtros.
 * Decide si el robot debe estar inactivo, operando o solo cerrando.
 */
void UpdateEAState()
{
    if(CheckDailyProfitGoalReached())
    {
        g_CurrentState = STATE_CLOSING_ONLY;
        return;
    }
    
    if(!IsTradingSessionActive())
    {
        // Si hay posiciones abiertas fuera de sesión, entramos en modo cierre.
        if(PositionsTotal() > 0)
        {
            g_CurrentState = STATE_CLOSING_ONLY;
        }
        else // Si no hay nada abierto, simplemente estamos inactivos.
        {
            g_CurrentState = STATE_IDLE;
        }
        return;
    }

    // Si pasamos todos los filtros, el estado es TRADING.
    g_CurrentState = STATE_TRADING;
}

/**
 * @brief Verifica si hemos alcanzado la meta de ganancias diaria.
 * @return true si la meta fue alcanzada, de lo contrario false.
 */
bool CheckDailyProfitGoalReached()
{
    MqlDateTime dt;
    TimeCurrent(dt);

    // Si es un nuevo día, reseteamos la bandera de la meta.
    if(dt.day_of_year != g_LastCheckDay)
    {
        g_ProfitGoalReached = false;
        g_LastCheckDay = dt.day_of_year;
        Print("Nuevo día de trading. Meta de ganancias reseteada.");
    }
    
    // Si la meta ya se alcanzó hoy, no necesitamos recalcular.
    if(g_ProfitGoalReached) return true;

    // Calculamos la ganancia de hoy
    double balance = AccountInfoDouble(ACCOUNT_BALANCE);
    double targetAmount = balance * (_DailyProfitGoalPercent / 100.0);
    double todayProfit = GetTodayProfit();

    if(todayProfit >= targetAmount)
    {
        g_ProfitGoalReached = true;
        PrintFormat("¡Meta de ganancia diaria alcanzada! Ganancia: %.2f, Meta: %.2f. Entrando en modo de solo cierre.", todayProfit, targetAmount);
        return true;
    }

    return false;
}


/**
 * @brief Verifica si el momento actual está dentro del horario y día de operación.
 * @return true si se permite operar, de lo contrario false.
 */
bool IsTradingSessionActive()
{
    // --- Filtro de Día ---
    if(_EnableDayFilter)
    {
        MqlDateTime dt;
        TimeCurrent(dt);
        bool isDayAllowed = false;
        if(dt.day_of_week == 1 && _WorkOnMonday) isDayAllowed = true;
        if(dt.day_of_week == 2 && _WorkOnTuesday) isDayAllowed = true;
        if(dt.day_of_week == 3 && _WorkOnWednesday) isDayAllowed = true;
        if(dt.day_of_week == 4 && _WorkOnThursday) isDayAllowed = true;
        if(dt.day_of_week == 5 && _WorkOnFriday) isDayAllowed = true;
        
        if(!isDayAllowed) return false; // No es un día de operación
    }

    // --- Filtro de Hora ---
    if(_EnableTimeFilter)
    {
        MqlDateTime dt;
        TimeCurrent(dt);
        if(dt.hour < _StartHour || dt.hour >= _EndHour)
        {
            return false; // No estamos en el horario de operación
        }
    }
    
    // Si pasó ambos filtros (o están deshabilitados), se permite operar.
    return true;
}


//+------------------------------------------------------------------+
//|          SECCIÓN 5: GESTIÓN DE OPERACIONES POR ZONA              |
//+------------------------------------------------------------------+

/**
 * @brief Dirige el flujo de trading basado en la zona de Fibonacci actual.
 * @param zone La zona operativa actual (BUY, SELL, RANGING, etc.).
 */
void ManageTradesByZone(ENUM_OPERATIVE_ZONE zone)
{
    switch(zone)
    {
        case ZONE_BUY:
            ManageBuyTrades();
            break;

        case ZONE_SELL:
            ManageSellTrades();
            break;

        case ZONE_RANGING:
            ManageBuyTrades();
            ManageSellTrades();
            break;
            
        case ZONE_OUT_OF_RANGE:
            Print("Precio fuera de rango. No se abren operaciones.");
            break;
    }
}

/**
 * @brief Gestiona toda la lógica para las operaciones de COMPRA.
 * Abre la operación inicial o añade a la parrilla si es necesario.
 */
void ManageBuyTrades()
{
    int buyPositions = CountOpenPositions(POSITION_TYPE_BUY);

    if(buyPositions == 0)
    {
        OpenInitialTrade(POSITION_TYPE_BUY);
    }
    else
    {
        CheckAndOpenGridTrade(POSITION_TYPE_BUY);
    }
}

/**
 * @brief Gestiona toda la lógica para las operaciones de VENTA.
 * Abre la operación inicial o añade a la parrilla si es necesario.
 */
void ManageSellTrades()
{
    int sellPositions = CountOpenPositions(POSITION_TYPE_SELL);

    if(sellPositions == 0)
    {
        OpenInitialTrade(POSITION_TYPE_SELL);
    }
    else
    {
        CheckAndOpenGridTrade(POSITION_TYPE_SELL);
    }
}


//+------------------------------------------------------------------+
//|            SECCIÓN 6: APERTURA DE OPERACIONES                    |
//+------------------------------------------------------------------+

/**
 * @brief Abre la primera operación (compra o venta) si no hay ninguna.
 * @param type El tipo de posición a abrir (POSITION_TYPE_BUY o POSITION_TYPE_SELL).
 */
void OpenInitialTrade(ENUM_POSITION_TYPE type)
{
    double price = (type == POSITION_TYPE_BUY) ? SymbolInfoDouble(_Symbol, SYMBOL_ASK) : SymbolInfoDouble(_Symbol, SYMBOL_BID);
    double tp = CalculateTakeProfitInPrice(type, price, _TakeProfitInCurrency);
    
    string typeStr = (type == POSITION_TYPE_BUY) ? "COMPRA" : "VENTA";
    PrintFormat("Abriendo %s inicial de %.2f lotes en %.5f con TP en %.5f", typeStr, _LotSize, price, tp);
    
    trade.PositionOpen(_Symbol, type, _LotSize, price, 0, tp, "Initial Trade");
}


/**
 * @brief Verifica si las condiciones para una operación de parrilla se cumplen y la abre.
 * @param type El tipo de posición a gestionar (POSITION_TYPE_BUY o POSITION_TYPE_SELL).
 */
void CheckAndOpenGridTrade(ENUM_POSITION_TYPE type)
{
    double lastPrice = GetLastPositionPrice(type);
    double currentPrice = (type == POSITION_TYPE_BUY) ? SymbolInfoDouble(_Symbol, SYMBOL_ASK) : SymbolInfoDouble(_Symbol, SYMBOL_BID);
    double pipStepInPrice = _PipStep * _Point;

    bool conditionMet = false;
    if(type == POSITION_TYPE_BUY && currentPrice <= lastPrice - pipStepInPrice)
    {
        conditionMet = true;
    }
    else if(type == POSITION_TYPE_SELL && currentPrice >= lastPrice + pipStepInPrice)
    {
        conditionMet = true;
    }
    
    if(conditionMet)
    {
        double lastLot = GetLastPositionLot(type);
        double newLot = NormalizeDouble(lastLot * _MartingaleMultiplier, 2);
        
        string typeStr = (type == POSITION_TYPE_BUY) ? "COMPRA" : "VENTA";
        PrintFormat("Abriendo %s de parrilla. Lote: %.2f", typeStr, newLot);

        // Abrir la nueva operación SIN Take Profit temporalmente
        trade.PositionOpen(_Symbol, type, newLot, currentPrice, 0, 0, "Grid Trade");
        
        // Una vez abierta, recalcular y modificar el TP para TODAS las posiciones
        UpdateCollectiveTakeProfit(type);
    }
}

//+------------------------------------------------------------------+
//|          SECCIÓN 7: CÁLCULOS Y FUNCIONES AUXILIARES              |
//+------------------------------------------------------------------+

/**
 * @brief Actualiza el Take Profit de todas las posiciones abiertas de un tipo.
 * Calcula el precio promedio y establece un TP común para el breakeven + ganancia.
 * @param type El tipo de posición a modificar (POSITION_TYPE_BUY o POSITION_TYPE_SELL).
 */
void UpdateCollectiveTakeProfit(ENUM_POSITION_TYPE type)
{
    double averagePrice = GetAveragePositionPrice(type);
    if(averagePrice == 0) return; // No hay posiciones de este tipo
    
    double newTP = CalculateTakeProfitInPrice(type, averagePrice, _TakeProfitInCurrency);
    
    // Iteramos por todas las posiciones y modificamos las que correspondan
    for(int i = PositionsTotal() - 1; i >= 0; i--)
    {
        if(posInfo.SelectByIndex(i) && posInfo.Symbol() == _Symbol && posInfo.Magic() == _MagicNumber)
        {
            if(posInfo.PositionType() == type)
            {
                trade.PositionModify(posInfo.Ticket(), 0, newTP);
            }
        }
    }
    PrintFormat("TP colectivo para %s actualizado a: %.5f", (type == POSITION_TYPE_BUY ? "COMPRAS" : "VENTAS"), newTP);
}


/**
 * @brief Calcula el precio de Take Profit basado en un monto en la moneda de la cuenta.
 * @param type El tipo de posición (BUY o SELL).
 * @param openPrice El precio de apertura (o el precio promedio para parrillas).
 * @param profitInCurrency El monto de ganancia deseado (ej: 0.50).
 * @return El precio absoluto para el Take Profit.
 */
double CalculateTakeProfitInPrice(ENUM_POSITION_TYPE type, double openPrice, double profitInCurrency)
{
    double tickValue = SymbolInfoDouble(_Symbol, SYMBOL_TICK_VALUE);
    double tickSize = SymbolInfoDouble(_Symbol, SYMBOL_TICK_SIZE);
    
    if (tickValue <= 0 || tickSize <= 0) return 0;

    // Calculamos cuántos "ticks" de ganancia necesitamos
    double ticksForProfit = profitInCurrency / tickValue;
    double priceOffset = ticksForProfit * tickSize;

    if(type == POSITION_TYPE_BUY)
    {
        return NormalizeDouble(openPrice + priceOffset, _Digits);
    }
    else // POSITION_TYPE_SELL
    {
        return NormalizeDouble(openPrice - priceOffset, _Digits);
    }
}

/**
 * @brief Calcula la ganancia total de hoy.
 * Suma los profits de todos los trades cerrados hoy.
 * @return El monto total de la ganancia de hoy.
 */
double GetTodayProfit()
{
    MqlDateTime from_dt, to_dt;
    TimeCurrent(to_dt); // Hasta ahora
    from_dt = to_dt;
    from_dt.hour = 0; from_dt.min = 0; from_dt.sec = 0; // Desde el inicio del día

    ulong from_time = StructToTime(from_dt);
    ulong to_time = StructToTime(to_dt);

    HistorySelect(from_time, to_time);
    
    double totalProfit = 0;
    for(uint i = 0; i < HistoryDealsTotal(); i++)
    {
        if(dealInfo.SelectByIndex(i) && dealInfo.Magic() == _MagicNumber)
        {
            // Solo contamos deals de entrada/salida que generan profit
            if(dealInfo.Entry() == DEAL_ENTRY_IN || dealInfo.Entry() == DEAL_ENTRY_OUT)
            {
                totalProfit += dealInfo.Profit() + dealInfo.Swap() + dealInfo.Commission();
            }
        }
    }
    return totalProfit;
}

// --- Funciones auxiliares para obtener información de posiciones ---

/** @brief Cuenta las posiciones abiertas de un tipo específico. */
int CountOpenPositions(ENUM_POSITION_TYPE type)
{
    int count = 0;
    for(int i = PositionsTotal() - 1; i >= 0; i--)
    {
        if(posInfo.SelectByIndex(i) && posInfo.Symbol() == _Symbol && posInfo.Magic() == _MagicNumber)
        {
            if(posInfo.PositionType() == type) count++;
        }
    }
    return count;
}

/** @brief Obtiene el precio de la última posición abierta de un tipo. */
double GetLastPositionPrice(ENUM_POSITION_TYPE type)
{
    double lastPrice = 0;
    datetime lastTime = 0;
    for(int i = PositionsTotal() - 1; i >= 0; i--)
    {
        if(posInfo.SelectByIndex(i) && posInfo.Symbol() == _Symbol && posInfo.Magic() == _MagicNumber)
        {
            if(posInfo.PositionType() == type && posInfo.Time() > lastTime)
            {
                lastTime = posInfo.Time();
                lastPrice = posInfo.PriceOpen();
            }
        }
    }
    return lastPrice;
}

/** @brief Obtiene el lotaje de la última posición abierta de un tipo. */
double GetLastPositionLot(ENUM_POSITION_TYPE type)
{
    double lastLot = 0;
    datetime lastTime = 0;
    for(int i = PositionsTotal() - 1; i >= 0; i--)
    {
        if(posInfo.SelectByIndex(i) && posInfo.Symbol() == _Symbol && posInfo.Magic() == _MagicNumber)
        {
            if(posInfo.PositionType() == type && posInfo.Time() > lastTime)
            {
                lastTime = posInfo.Time();
                lastLot = posInfo.Volume();
            }
        }
    }
    return lastLot;
}

/** @brief Calcula el precio promedio ponderado por lote para un tipo de posición. */
double GetAveragePositionPrice(ENUM_POSITION_TYPE type)
{
    double totalLot = 0;
    double weightedPriceSum = 0;
    
    for(int i = PositionsTotal() - 1; i >= 0; i--)
    {
        if(posInfo.SelectByIndex(i) && posInfo.Symbol() == _Symbol && posInfo.Magic() == _MagicNumber)
        {
            if(posInfo.PositionType() == type)
            {
                totalLot += posInfo.Volume();
                weightedPriceSum += posInfo.PriceOpen() * posInfo.Volume();
            }
        }
    }

    if(totalLot == 0) return 0;
    return weightedPriceSum / totalLot;
}
