//+------------------------------------------------------------------+
//|                                                LogicaEntrada.mqh |
//|        Módulo que decide qué hacer según la zona operativa       |
//+------------------------------------------------------------------+
#property strict

class LogicaEntrada
{
public:
    static void ExecuteTradingLogic()
    {
        // Este módulo actúa como un director de orquesta,
        // llamando al módulo de gestión de órdenes según la zona.
        switch(g_CurrentZone)
        {
            case ZONE_BUY:
                GestionOrdenes::ManageTrades(ORDER_TYPE_SELL, true); // Cerrar ventas
                GestionOrdenes::ManageTrades(ORDER_TYPE_BUY, g_is_in_closing_only_mode);
                break;
            
            case ZONE_SELL:
                GestionOrdenes::ManageTrades(ORDER_TYPE_BUY, true);  // Cerrar compras
                GestionOrdenes::ManageTrades(ORDER_TYPE_SELL, g_is_in_closing_only_mode);
                break;

            case ZONE_RANGING:
                GestionOrdenes::ManageTrades(ORDER_TYPE_BUY, g_is_in_closing_only_mode);
                GestionOrdenes::ManageTrades(ORDER_TYPE_SELL, g_is_in_closing_only_mode);
                break;

            case ZONE_OUT_OF_RANGE:
                // Estamos fuera de rango, no abrimos nada nuevo.
                // Gestionamos para cerrar todo lo que pueda estar abierto.
                if(!g_is_in_closing_only_mode) Print("Fuera de rango. No se abren nuevas operaciones.");
                GestionOrdenes::ManageTrades(ORDER_TYPE_BUY, true);
                GestionOrdenes::ManageTrades(ORDER_TYPE_SELL, true);
                break;
        }
    }
};