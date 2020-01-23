// Extremely simple Option system //////////////////////////////////////////
#include <contract.c>

#define PREMIUM	2
#define WEEKS		6 // expiration
//#define ROLL
#define DOWNLOAD
//#define REAL_DATA

int i;
var Price;

CONTRACT* findCall(int Expiry,var Premium)
{
	for(i=0; i<50; i++) {
		if(!contract(CALL,Expiry,Price+0.5*i)) return 0;
		if(between(ContractBid,0.1,Premium)) 
			return ThisContract;
	}
	return 0;
}

CONTRACT* findPut(int Expiry,var Premium)
{
	for(i=0; i<50; i++) {
		if(!contract(PUT,Expiry,Price-0.5*i)) return 0;
		if(between(ContractBid,0.1,Premium)) return ThisContract;
	}
	return 0;
}

void run() 
{
	StartDate = 2012;
	EndDate = 2018;
	BarPeriod = 1440;
	if(is(TRADEMODE)) BarZone = ET; // EOD history is already ET
	BarOffset = 15*60+20; // trade at 15:20 ET
	LookBack = 1;
	PlotMode = 0;
	set(PLOTNOW);
	set(PRELOAD|LOGFILE);
	//LogNumber = 2;

	assetList("AssetsIB");
#ifdef DOWNLOAD
	assetHistory("SPY.US",FROM_STOOQ|UNADJUSTED);
#endif
	asset("SPY");
	Multiplier = 100;

// load today's contract chain
	Price = priceClose();
#ifdef REAL_DATA
	int N = contractUpdate("SPY",0,CALL|PUT);
#else
	int N = contractUpdate("SPYa",0,CALL|PUT); // artificial options data
#endif
	printf("#\n%i contracts updated",N); 

// check for in-the-money and roll 	
#ifdef ROLL
	for(open_trades) {
		var Loss = -TradeProfit/Multiplier;
		if(TradeIsCall && Price >= TradeStrike) {
			exitTrade(ThisTrade);
			printf("#\nRoll %.1f at %.2f Loss %.2f",(var)TradeStrike,Price,(var)TradeProfit);
			CONTRACT* C = findCall(WEEKS*7,Loss*1.1);
			if(C) {
				MarginCost = 0.15*Price - (C->fStrike-Price);
				enterShort();
			}
		} else if(TradeIsPut && Price <= TradeStrike) {
			exitTrade(ThisTrade);
			printf("#\nRoll %.1f at %.2f Loss %.2f",(var)TradeStrike,Price,(var)TradeProfit);
			CONTRACT* C = findPut(WEEKS*7,Loss*1.1);
			if(C) { 
				MarginCost = 0.15*Price - (Price-C->fStrike);
				enterShort();
			}
		}
	}
#endif
	
// all expired? enter new options
	if(!NumOpenShort) { 
		CONTRACT *Call = findCall(WEEKS*7,PREMIUM); 
		CONTRACT *Put = findPut(WEEKS*7,PREMIUM); 		
		if(Call && Put) {
			MarginCost = 0.5*(0.15*Price - min(Call->fStrike-Price,Price-Put->fStrike));
			contract(Call); enterShort();
			contract(Put); enterShort();
		}
	}
}