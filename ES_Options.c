//Every Monday at market open, sell a ES 1-week Short Strangle. 
//Strangle strikes are determined by a given Delta (use contractStrike() function).

//Hedge with a long ES position with equal expiration and equal number of contracts when the price moves above the call strike and 
//order flow imbalance indicates rising prices. 

//Opposite for put.

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
//		(6): Do (4) and (5) only when orderflow imbalance --> imbalance not defined --> use total cumulative volume at the range
//		(7): Opposite for put --> when price below put strike (PLUS imbalance towards falling prices)- sell 1-week Normal

#include <r.h>
#include <contract.c>

#define ASSETLIST					"AssetsFix"
#define SYMBOL 					"SPY"
//#define ASSETLIST				"AssetsCMSW"
//#define SYMBOL 					"SPX500"

#define HISTORY					".t8"		// historical data file
#define STARTDATE					20180101
#define ENDDATE					20180603	
#define BARPERIOD					1440		//	bar period in minutes

#define RISKFREE					0.02		// risk free rate
#define EXPIREWEEKS				1			// weeks expiration
#define DELTACALL					16. 		// delta of the call contract
#define DELTAPUT					16.		// delta of the put contract

#define CONTRACTSCALL			1			// contract number of the call combo leg
#define CONTRACTSPUT				1			// contract number of the put combo leg

#define ZERO_COST								// test with no spread and commission

#define RANGE						0.003				// order book range, +/-0.1%

#define EXITHOURS					24


function udlLong()
{
	int numUdlLong;
	for(current_trades) 
	{
		if(!(TradeIsCall or TradeIsPut))
		{
			if(TradeIsLong) numUdlLong = numUdlLong+1;
		}
	}
	
	return numUdlLong;
}

function udlShort()
{
	int numUdlShort;
	
	for(current_trades) 
	{
		if(!(TradeIsCall or TradeIsPut))
		{
			if(TradeIsShort) numUdlShort = numUdlShort+1;
		}
	}
	
	return numUdlShort;
}

function run()
{
	set(PLOTNOW);
	
	StartDate = STARTDATE;
	EndDate = ENDDATE;
	
	BarPeriod = BARPERIOD;
//	LookBack = 0;

//	NumCores = -1;
//	Outlier = 2;
//	MonteCarlo = 0;
//
//// set up walk-forward parameters
//	int TST = 4*1440/BarPeriod; //number of days in test period
//	int TRN = 8*1440/BarPeriod; //number of days in training period
//	DataSplit = 100*TRN/(TST+TRN);
//	WFOPeriod = TST+TRN;	
//	if(ReTrain) 
//	{
//		SelectWFO = -1;
//	}

	assetList(ASSETLIST);
	History = HISTORY;	//HISTORY; 
	
	asset(SYMBOL);
	
	contractUpdate(Asset,0,CALL|PUT);
	var CenterPrice = contractUnderlying();
	var HistVol = Volatility(series(price()),20);
	
	Multiplier = 100;
	Commission = 1.0/Multiplier;

// trading cost
	Slippage = 0;
#ifdef ZERO_COST
	Commission = Spread = 0;
#endif
	
	var deltaCall = optimize(DELTACALL,DELTACALL-5,DELTACALL+5,1);
	var deltaPut = optimize(DELTAPUT,DELTAPUT-5,DELTAPUT+5,1);
	
	int ExpireDays = optimize(EXPIREWEEKS*7,7,6*7,1);
	
	//Setup and enter Strangle
	if(NumOpenShort == 0 and NumOpenLong == 0 and dow()==1) 
	{
		var CallStrikeCall = contractStrike(CALL,ExpireDays,CenterPrice,HistVol,RISKFREE,deltaCall/100.);
		var CallStrikePut = contractStrike(PUT,ExpireDays,CenterPrice,HistVol,RISKFREE,deltaPut/100.);
	
//		Setup combo
		if(!combo(
			contract(CALL,ExpireDays,CallStrikeCall),CONTRACTSCALL, 
			contract(PUT,ExpireDays,CallStrikePut),CONTRACTSPUT,
			0,0,0,0))
		{
			printf("#\n%s - cannot find matching %s Strangle!",Asset,Algo);
//			continue;
		}

	// check for valid prices		
		int i;
		for(i=1; i<=2; i++) 
		{
			comboLeg(i);
			if((i == 1 && ContractBid < 0.05)
				|| (i != 1 && ContractAsk < 0.05))
			{
//				printf("#\nNo valid price for %s %s leg %i",Asset,Algo,i);
				break;
			}
		}
//		if(i < 3) continue;

		for(i=1;i<=2;i++)
			ThisTrade = enterShort(1*comboLeg(i));
	}
		
//temporary bool to register risingPrices
	bool risingPrices;

#ifdef TRADE_IMBALANCE
//	OrderFlow imbalance ////////////////////////////////////////////////////////////////////////////////////////////////////
//	data //////////////////////////////////////////////////
	vars Prices = series(CenterPrice);
//	var Features[27]; // Features to be sent to the network
   int M = 0;

// load today's order book ///////////////////////////
	M = orderUpdate(SYMBOL,1);
	T2* Quotes = dataStr(1,OrderRow,0);
	//printf("\nOrderbook: %i quotes",M);
	var Distance = RANGE*CenterPrice; // price range
	int N2 = orderCVD(Quotes,M,Distance);
	//printf(", %i in range",N2);
	
	var askPriceLevel = CenterPrice+Distance;
	var bidPriceLevel = CenterPrice-Distance;
	
	//Check if price is dominated by rising or falling prices
	var askVol = cpd(askPriceLevel);
	var bidVol = cpd(-bidPriceLevel);
	
	if(bidVol>askVol) risingPrices=true;
#endif
	
	static var CallStrike, PutStrike, contractExpiry=0.;
	for(current_trades) 
	{
		if(TradeIsCall) 
		{
			CallStrike = TradeStrike;
//			if(dow()==2) printf("\nCall: Contract days: %.2f\n Strike: %.2f",contractDays(ThisTrade),TradeStrike);
		}
		else if(TradeIsPut)
		{
			PutStrike = TradeStrike;	
			contractExpiry = contractDays(ThisTrade);
//			if(dow()==2) printf("\nPut: Contract days: %.2f\n Strike: %.2f",contractDays(ThisTrade),TradeStrike);
		} 
	}
	
	risingPrices=true;
	
////	if(dow()==2) printf("\n%i",NumOpenTotal);

	//removed NumOpenTotal - replaced with strike check and udlLong/short
//		if(NumOpenTotal==2 and contractExpiry > EXITHOURS/24.)
	if(CallStrike!=0 and PutStrike!=0 and contractExpiry > EXITHOURS/24.)
	{
		if(risingPrices==true and CenterPrice>CallStrike and udlLong()==0)
		{
			contract(0); //ONLY FOR PRILIMINARY TESTING
	//		contract(FUTURE,contractExpiry,0);
	
			enterLong(CONTRACTSCALL);
	
//			printf("\nImbalance towards rising prices and prices higher than call strike. \nEnter long underlying.");
	//		printf("\nEnter %i long contracts with expiry of %.0f days.",CONTRACTSCALL,contractExpiry);
//			plot("hedge call",CenterPrice*1.1,MAIN|DOT,RED);
		}
	//	else if(risingPrices==false and CenterPrice<PutStrike and udlShort()==0)
		else if(risingPrices==true and CenterPrice<PutStrike and udlShort()==0) //ONLY FOR PRIMARY TESTING
		{
			contract(0); //ONLY FOR PRILIMINARY TESTING
	//		contract(FUTURE,contractExpiry,0);
	
			enterShort(CONTRACTSPUT);
//			printf("\nImbalance towards falling prices and prices lower than put strike. \nEnter short underlying.");
	//		printf("\nEnter %i short contracts with expiry of %.0f days.",CONTRACTSPUT,contractExpiry);
//			plot("hedge put",CenterPrice*0.9,MAIN|DOT,BLUE);
		}
	}
	
	for(current_trades)
	{
		if(!(TradeIsCall or TradeIsPut) and TradeIsOpen)
		{
			if(CenterPrice<CallStrike or contractExpiry <= EXITHOURS/24.) exitLong("u");
			if(CenterPrice>PutStrike or contractExpiry <= EXITHOURS/24.) exitShort("u");
		}
	}
	
//	plot("CenterPrice",CenterPrice,NEW,RED);
//	plot("CallStrike",CallStrike,NEW,RED);
//	plot("PutStrike",PutStrike,NEW,RED);
	
	
//	if(CenterPrice<CallStrike) //check CallStrike !=0 too?
//	{
//		contract(0);
//		exitLong("udl");
//	}
////	plot("CenterPrice",CenterPrice,NEW,BLUE);
////	if(PutStrike>0) plot("PutStrike",PutStrike,0,RED);
// 	
// 	if(CenterPrice>PutStrike) 
//	{
////		plot("hedge put",CenterPrice*0.9,MAIN|DOT,BLUE);
//		contract(0);
//		exitShort("udl");
//	}
	
	plot("contractExpiry",contractExpiry,NEW,RED);
//	if(contractExpiry <= EXITHOURS/24.) 
//	{
////		plot("hedge put",CenterPrice*0.9,MAIN|DOT,BLUE);
//		int longs=udlLong();
//		int shorts=udlShort();
//////		//DOES NOT WORK
////		contract(0);
////		exitLong("exp"); 
////		exitShort("exp");
////		//replaced with contract(0) above -->
//		for(current_trades)
//		{
//			if(!(TradeIsCall or TradeIsPut) or (NumOpenTotal==(longs+shorts))) //probably not needed
//			{
//				exitLong(); 
//				exitShort();
//			}
//				
//		}
//	}
//	
//	if(dow()==2) printf("\n %i %i",NumOpenLong,NumOpenShort);

}