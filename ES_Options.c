//Every Monday at market open, sell a ES 1-week Short Strangle. 

//Hedge with a long ES position with equal expiration and equal number of contracts when the price moves above the call strike and 
//order flow imbalance indicates rising prices. 

//Opposite for put.


//Strangle strikes are determined by a given Delta (use contractStrike() function).
//
//Order flow imbalance is detected by comparing the ask volume with the bid volume. Higher ask volume indicates rising prices.
//
//Close the hedging position when price moves back to the neutral zone between call and put strikes. 
//Use a hysteresis (small factor, like 0.0025) to avoid whipsaw trades.
//
//Do a walk-Forward optimization for Delta, imbalance threshold, hedge hysteresis threshold, and expiration (1..6 weeks).
//
//Script must be tested in backtest and in live trading with a IB demo account. Use the GET_BOOK command.


//PLAN:
//		(1): Plot a short strangle (add a long ES position --> long option or underlying??)
//		(2): Sell 1-week Strangle 
//		(3): Do (2) only on Monday
//		(4): Long 1-week Normal (Option or Underlying? --> see (1)) --> I guess it's the option
//		(5): Do (4) only when the price moves above call strike
//		(6): Do (4) and (5) only when orderflow imbalance --> imbalance not defined --> use the outer 25% bucket for now
//		(7): Opposite for put --> when price below put strike (PLUS imbalance)- sell 1-week Normal