//+------------------------------------------------------------------+
//|                                               GestionOrdenes.mqh |
//|     Módulo para abrir, cerrar y gestionar órdenes y riesgo       |
//+------------------------------------------------------------------+
#property strict

#include <Arrays\ArrayObj.h>

class GestionOrdenes
{
private:
    // --- FUNCIÓN PRIVADA DE AYUDA ---
    static double GetMonetaryTakeProfitInPoints(double lot_size)
    {
        if (_TP_Mode == IN_PIPS)
        {
            return _TakeProfitValue * symbol_info.Point();
        }
        
        // Modo IN_DOLLARS
        double tick_value = symbol_info.TickValue();
        double tick_size = symbol_info.TickSize();
        
        if (tick_value <= 0 || tick_size <= 0) return 0;
        
        // Valor en $ de un punto
        double point_value = tick_value / tick_size * symbol_info.Point();
        
        // Puntos necesarios para la ganancia
        double points_needed = _TakeProfitValue / (point_value * lot_size);
        
        return points_needed * symbol_info.Point();
    }

public:
    static void ManageTrades(ENUM_ORDER_TYPE direction, bool close_only_mode)
    {
        CArrayObj* trade_list = GetOpenPositions(direction);
        int total_trades = trade_list.Total();

        // --- Lógica de Apertura Inicial ---
        if (total_trades == 0 && !close_only_mode)
        {
            double tp_points = GetMonetaryTakeProfitInPoints(_InitialLotSize);
            double price = (direction == ORDER_TYPE_BUY) ? symbol_info.Ask() : symbol_info.Bid();
            double tp_price = (direction == ORDER_TYPE_BUY) ? price + tp_points : price - tp_points;

            if (direction == ORDER_TYPE_BUY)
                trade.Buy(_InitialLotSize, _Symbol, 0, 0, tp_price, "Buy by ElProfeXau EA");
            else
                trade.Sell(_InitialLotSize, _Symbol, 0, 0, tp_price, "Sell by ElProfeXau EA");
            
            delete trade_list;
            return;
        }
        
        if (total_trades > 0)
        {
            // --- Lógica de Grid y Martingala ---
            if (!close_only_mode)
            {
                CHistoryOrderInfo last_trade;
                last_trade.SelectByIndex(trade_list.Total() - 1); // La última orden es la más reciente
                
                double last_price = last_trade.PriceOpen();
                double current_price = (direction == ORDER_TYPE_BUY) ? symbol_info.Ask() : symbol_info.Bid();
                double distance_points = (direction == ORDER_TYPE_BUY) ? last_price - current_price : current_price - last_price;

                if (distance_points >= _PipStepValue * symbol_info.Point())
                {
                    double new_lot = _InitialLotSize;
                    if (_EnableMartingale && total_trades >= _MartingaleStartLevel)
                    {
                        CHistoryOrderInfo prev_trade;
                        prev_trade.SelectByIndex(trade_list.Total() - 1);
                        new_lot = prev_trade.Lots() * _MartingaleMultiplier;
                        // Aquí se podría añadir un límite máximo de lotaje
                    }
                    
                    if (direction == ORDER_TYPE_BUY)
                        trade.Buy(new_lot, _Symbol, 0, 0, 0, "Buy Grid");
                    else
                        trade.Sell(new_lot, _Symbol, 0, 0, 0, "Sell Grid");
                    
                    // Forzar recalcular TP inmediatamente después de abrir
                    RecalculateAndModifyTP(direction);
                }
            }
            
            // --- Recalcular y Ajustar TP ---
            RecalculateAndModifyTP(direction);
        }
        
        delete trade_list;
    }

    static void RecalculateAndModifyTP(ENUM_ORDER_TYPE direction)
    {
        CArrayObj* trade_list = GetOpenPositions(direction);
        int total_trades = trade_list.Total();
        if(total_trades == 0) 
        {
            delete trade_list;
            return;
        }

        double total_lots = 0;
        double weighted_price_sum = 0;

        for(int i = 0; i < total_trades; i++)
        {
            CHistoryOrderInfo order;
            order.SelectByIndex(i);
            total_lots += order.Lots();
            weighted_price_sum += order.PriceOpen() * order.Lots();
        }

        double avg_price = weighted_price_sum / total_lots;
        double tp_points = GetMonetaryTakeProfitInPoints(total_lots);
        double new_tp_price = (direction == ORDER_TYPE_BUY) ? avg_price + tp_points : avg_price - tp_points;
        
        // Modificar TP de todas las órdenes en esta dirección
        for(int i = 0; i < total_trades; i++)
        {
            CHistoryOrderInfo order;
            order.SelectByIndex(i);
            trade.PositionModify(order.Ticket(), order.StopLoss(), new_tp_price);
        }
        
        delete trade_list;
    }
    
    // Función de ayuda para obtener posiciones abiertas por dirección y magic number
    static CArrayObj* GetOpenPositions(ENUM_ORDER_TYPE direction)
    {
        CArrayObj* list = new CArrayObj();
        for(int i = PositionsTotal() - 1; i >= 0; i--)
        {
            if(PositionGetSymbol(i) == _Symbol && PositionGetInteger(POSITION_MAGIC) == _MagicNumber)
            {
                if(PositionGetInteger(POSITION_TYPE) == direction)
                {
                    CHistoryOrderInfo* order = new CHistoryOrderInfo();
                    order.SelectByIndex(i);
                    list.Add(order);
                }
            }
        }
        return list;
    }
};