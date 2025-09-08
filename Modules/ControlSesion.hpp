//+------------------------------------------------------------------+
//|                                              ControlSesion.mqh   |
//|                     Módulo para el control de la sesión          |
//+------------------------------------------------------------------+
#property strict

class ControlSesion
{
public:
    static bool IsWithinTradingWindow()
    {
        // Si ambos filtros están deshabilitados, siempre operamos.
        if (!_EnableDayFilter && !_EnableTimeFilter)
        {
            return true;
        }

        MqlDateTime now;
        TimeToStruct(TimeCurrent(), now);

        // 1. Filtrado por día de la semana
        if (_EnableDayFilter)
        {
            bool is_day_allowed = false;
            switch(now.day_of_week)
            {
                case 1: is_day_allowed = _TradeOnMonday; break;
                case 2: is_day_allowed = _TradeOnTuesday; break;
                case 3: is_day_allowed = _TradeOnWednesday; break;
                case 4: is_day_allowed = _TradeOnThursday; break;
                case 5: is_day_allowed = _TradeOnFriday; break;
                default: is_day_allowed = false; // Sábado y Domingo
            }
            if (!is_day_allowed) return false;
        }

        // 2. Filtrado por hora del día (UTC)
        if (_EnableTimeFilter)
        {
            if (now.hour < _StartTimeUTC || now.hour >= _EndTimeUTC)
            {
                return false;
            }
        }
        
        // Si pasó todos los filtros, está dentro de la ventana.
        return true;
    }
};