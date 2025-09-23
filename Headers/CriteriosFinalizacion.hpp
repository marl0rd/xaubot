//+------------------------------------------------------------------+
//|                                       CriteriosFinalizacion.mqh |
//|     Módulo para verificar si se debe detener la operativa        |
//+------------------------------------------------------------------+
#property strict

class CriteriosFinalizacion
{
public:
    static void CheckForDayReset()
    {
        datetime today = TimeCurrent() - (TimeCurrent() % 86400);
        if (g_trading_day_start != today)
        {
            Print("Nuevo día de trading detectado. Reiniciando contadores.");
            g_trading_day_start = today;
            g_daily_goal_reached = false;
            g_balance_at_start_of_day = AccountInfoDouble(ACCOUNT_BALANCE);
        }
    }

    static bool CheckDailyProfitTarget()
    {
        if (_DailyProfitTarget <= 0) return false;

        HistorySelect(g_trading_day_start, TimeCurrent());
        double daily_profit = 0;
        uint total_deals = HistoryDealsTotal();

        for(uint i = 0; i < total_deals; i++)
        {
            ulong ticket = HistoryDealGetTicket(i);
            if (HistoryDealGetInteger(ticket, DEAL_MAGIC) == _MagicNumber)
            {
                if (HistoryDealGetInteger(ticket, DEAL_ENTRY) == DEAL_ENTRY_OUT) // Solo transacciones de cierre
                {
                    daily_profit += HistoryDealGetDouble(ticket, DEAL_PROFIT);
                }
            }
        }
        
        double target_amount = g_balance_at_start_of_day * (_DailyProfitTarget / 100.0);
        
        return (daily_profit >= target_amount);
    }
};