#property copyright "m0 / El ProfeXau"
#property link      ""
#property version   "1.0"

#include <Trade\Trade.mqh>
#include <Trade\SymbolInfo.mqh>

enum ENUM_TRADE_DIRECTION
{
    NONE,       // Buscando nada
    BUY_ONLY,   // Buscando compras
    SELL_ONLY   // Buscando ventas
};

enum ENUM_BASE_TIMEFRAME
{
    H4,     // Cuatro horas antiores
    DAILY,  // Del dia anterior
    WEEKLY, // De la semana pasada
    MONTHLY // Del mes pasado
};

//--- PARÁMETROS DE ENTRADA (INPUTS)
//--- Generales
input ulong               _MagicNumber        = 1666;    // ID

//--- Estrategia
input ENUM_BASE_TIMEFRAME _BaseTimeframe      = WEEKLY;  // Ventana de tiempo
input string              _FibonacciLevelsStr = "0.0,0.236,0.382,0.5,0.618,0.786,1.0"; // Nivel


//--- VARIABLES GLOBALES
CTrade               trade;
CSymbolInfo          symbol_info;
ENUM_TRADE_DIRECTION g_BaseDirection = NONE;
double               g_fib_percentages[];
double               g_fib_levels[];
datetime             g_last_period_candle_time = 0;
double               g_pip_value;
int                  g_slipMax = 10;

int OnInit()
{
    // --- Configuración Inicial ---
    trade.SetExpertMagicNumber(_MagicNumber);
    trade.SetDeviationInPoints(g_slipMax);
    symbol_info.Name(_Symbol);
    g_pip_value = symbol_info.Point() * 10;

    // --- Procesamiento de los niveles Fibonacci ---
    // Esto se hace solo una vez al iniciar.
    string temp_string_array[];
    StringSplit(_FibonacciLevelsStr, ',', temp_string_array);
    int total_levels = ArraySize(temp_string_array);
    ArrayResize(g_fib_percentages, total_levels);
    for(int i = 0; i < total_levels; i++)
    {
        g_fib_percentages[i] = StringToDouble(temp_string_array[i]);
    }

    Print("EA Inicializado. El análisis comenzará en la próxima vela H1.");
    // NOTA: Hemos eliminado la llamada a UpdateBasePeriodAnalysis() de aquí.
    // La inicialización no debe ejecutar la lógica principal.
    return(INIT_SUCCEEDED);
}

void OnTick()
{
    static datetime last_bar_time_h1 = 0;
    datetime        current_bar_time_h1 = (datetime)SeriesInfoInteger(_Symbol, PERIOD_H1, SERIES_LASTBAR_DATE);

    if(last_bar_time_h1 != current_bar_time_h1)
    {
        last_bar_time_h1 = current_bar_time_h1;
        UpdateBasePeriodAnalysis();
        if(PositionsTotal() == 0)
        {
            //AnalyzeH1ForWyckoffEntry();
        }
    }
}

void UpdateBasePeriodAnalysis()
{
    // 1. CALCULAR LOS MARCOS DE TIEMPO MANUALMENTE.
    datetime prev_period_start, prev_period_end;
    if(!GetPeriodTimeframes(prev_period_start, prev_period_end))
    {
        // No se pudo calcular el marco de tiempo, salimos.
        return;
    }

    // 2. EVITAR RE-CÁLCULOS INNECESARIOS
    // Si la última vela que analizamos es la misma que la actual, no hay nada nuevo que hacer.
    if(g_last_period_candle_time == prev_period_start) { return; }

    Print("------------------------------------------------------");
    Print("Nuevo periodo detectado (", EnumToString(_BaseTimeframe), "). Analizando...");
    
    // --- NUEVO: Lógica de Limpieza Robusta ---
    // Antes de dibujar un nuevo Fibo, nos aseguramos de borrar CUALQUIER Fibo anterior
    // que este bot pudiera haber dibujado, sin importar el timeframe que tuviera antes.
    for(int i = H4; i <= MONTHLY; i++) // Itera a través de todos los timeframes posibles en nuestra enum
    {
        string old_object_name = "FiboRetracement_" + EnumToString((ENUM_BASE_TIMEFRAME)i);
        ObjectDelete(0, old_object_name);
    }

    // 3. OBTENER LOS DATOS DE PRECIO
    double period_high, period_low, period_open, period_close;
    MqlRates rates[];
    
    ENUM_TIMEFRAMES granular_tf = PERIOD_D1;
    if(_BaseTimeframe == DAILY || _BaseTimeframe == H4)
    {
        granular_tf = PERIOD_H1;
    }

    int copied_rates = CopyRates(_Symbol, granular_tf, prev_period_start, prev_period_end, rates);

    if(copied_rates <= 0)
    {
        Print("Error: No hay datos históricos disponibles para este rango. Se reintentará.");
        return;
    } 
    
    // Si llegamos aquí, tuvimos éxito. Marcamos el periodo como procesado.
    g_last_period_candle_time = prev_period_start;

    period_open = rates[0].open;
    period_close = rates[copied_rates - 1].close;
    period_high = rates[0].high;
    period_low = rates[0].low;
    for(int i = 1; i < copied_rates; i++)
    {
        if(rates[i].high > period_high) period_high = rates[i].high;
        if(rates[i].low < period_low) period_low = rates[i].low;
    }
    Print("Datos del Periodo Obtenidos -> H: ", period_high, ", L: ", period_low);

    // 4. DETERMINAR DIRECCIÓN Y DIBUJAR
    datetime current_period_start = prev_period_end;

    if(period_close > period_open)
    {
        g_BaseDirection = BUY_ONLY;
        CalculateFibonacciLevels(period_low, period_high);
        DrawFibonacciObject(prev_period_start, period_low, current_period_start, period_high);
    }
    else if(period_close < period_open)
    {
        g_BaseDirection = SELL_ONLY;
        CalculateFibonacciLevels(period_high, period_low);
        DrawFibonacciObject(prev_period_start, period_high, current_period_start, period_low);
    }
    else
    {
        g_BaseDirection = NONE;
    }
    Print("Análisis completado. Dirección para el periodo: ", EnumToString(g_BaseDirection));
    Print("------------------------------------------------------");
}

bool GetPeriodTimeframes(datetime &out_start, datetime &out_end)
{
    MqlDateTime tm;
    datetime now = TimeCurrent();
    TimeToStruct(now, tm);
    
    if(_BaseTimeframe == H4)
    {
        // Lógica para H4 (Esta parte estaba correcta)
        tm.hour = (tm.hour / 4) * 4;
        tm.min = 0; tm.sec = 0;
        datetime current_h4_start = StructToTime(tm);

        out_end = current_h4_start;
        out_start = out_end - (4 * 3600); // 4 horas * 3600 segundos/hora
        return true;
    }

    if(_BaseTimeframe == DAILY)
    {
        // Lógica para DAILY
        tm.hour = 0; tm.min = 0; tm.sec = 0;
        datetime today_start = StructToTime(tm);

        out_end = today_start;
        
        // CORRECCIÓN: Restamos solo 86400 segundos (1 día), no 24 días.
        out_start = out_end - 86400; 
        return true;
    }
    
    if(_BaseTimeframe == WEEKLY)
    {
        // Lógica para WEEKLY (Esta parte estaba correcta)
        int days_to_subtract = tm.day_of_week - 1; 
        if(days_to_subtract < 0) days_to_subtract = 6;
        
        datetime current_week_monday = now - (days_to_subtract * 86400);
        
        TimeToStruct(current_week_monday, tm);
        tm.hour = 0; tm.min = 0; tm.sec = 0;
        
        out_end = StructToTime(tm);
        out_start = out_end - (7 * 86400);
        return true;
    }
    
    if(_BaseTimeframe == MONTHLY)
    {
        // Lógica para MONTHLY (Esta parte estaba correcta)
        tm.day = 1; tm.hour = 0; tm.min = 0; tm.sec = 0;
        datetime current_month_start = StructToTime(tm);
        out_end = current_month_start;

        if(tm.mon == 1) { tm.mon = 12; tm.year -= 1; }
        else { tm.mon -= 1; }
        out_start = StructToTime(tm);
        return true;
    }

    return false;
}

void CalculateFibonacciLevels(double start_price, double end_price)
{
    double range = MathAbs(end_price - start_price);
    int total_levels = ArraySize(g_fib_percentages);
    ArrayResize(g_fib_levels, total_levels);

    for(int i = 0; i < total_levels; i++)
    {
        double level_percentage = g_fib_percentages[i];
        if(start_price < end_price)
        {
            g_fib_levels[i] = end_price - (range * level_percentage);
        }
        else
        {
            g_fib_levels[i] = end_price + (range * level_percentage);
        }
    }
    ArraySort(g_fib_levels);
    Print("Cálculo de niveles de precio Fibo completado.");
}

void DrawFibonacciObject(datetime time1, double price1, datetime time2, double price2)
{
    Print("Dibujando fib...");
    string object_name = "FiboRetracement_" + EnumToString(_BaseTimeframe);
    ObjectDelete(0, object_name);

    if(!ObjectCreate(0, object_name, OBJ_FIBO, 0, time1, price1, time2, price2))
    {
        Print("Error al crear el objeto Fibonacci. Código: ", GetLastError());
        return;
    }

    int total_levels = ArraySize(g_fib_percentages);
    for(int i = 0; i < total_levels; i++)
    {
        ObjectSetDouble(0, object_name, OBJPROP_LEVELVALUE, i, g_fib_percentages[i]);
        ObjectSetString(0, object_name, OBJPROP_LEVELTEXT, i, StringFormat("%.1f%%", g_fib_percentages[i] * 100));
    }

    ObjectSetInteger(0, object_name, OBJPROP_COLOR, clrGold);
    ObjectSetInteger(0, object_name, OBJPROP_WIDTH, 1);
    ObjectSetInteger(0, object_name, OBJPROP_RAY_RIGHT, true);
    ObjectSetInteger(0, object_name, OBJPROP_SELECTABLE, false);
    ObjectSetInteger(0, object_name, OBJPROP_BACK, true);

    ChartRedraw();
    Print("Objeto Fibonacci (", EnumToString(_BaseTimeframe), ") dibujado/actualizado.");
}
