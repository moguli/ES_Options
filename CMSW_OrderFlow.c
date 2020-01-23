// OrderFlow deep learning system ///////////////////////
// 9/2018 D.Lindberg / oP group Germany
// 8/2019 P.Noebauer
///////////////////////////////////////////////////////

#define ASSETLIST			"AssetsCMSW"
#define SYMBOL 			"ESU"
//#define HISTORY			"05.t1"	// historical data file
#define STARTDATE			20190401
#define ENDDATE			20190630	
#define BARPERIOD			15			//	bar period in minutes
//#define FILTER				0.1		// prediction filter; higher -> less trades 
#define FILTER				0.01		// prediction filter; higher -> less trades 
#define RANGE				0.003		// order book range, +/-0.1%
#define ZERO_COST						// test with no spread and commission
#define SHORTLONG						// allow short positions
#define BIDDISTMULT		-1			// usually -1 to get the volume for the bid , +/-1		
#define BIDLEVOFFSET		0			// offset to make a comparison with ask level above/below
//#define DO_SIGNALS  				// generate sample set in Train mode

/*
The system uses 27 signals for deep learning futures prediction. 

1. Volume Ask-Bid ratio profile ("orderflow") of at least 2 candles
2. Normalized volume profile ("footprint") of at least 2 candles
3. Ask-Bid Spread of at least 2 candles, in percent of the price
4. Volatility (High-Low Ranges) of at least 2 candles, in percent of the price
5. Ask price change of at least 3 candles, in percent 
*/

#include <r.h>

function run()
{
	set(PEEK|RULES|PLOTNOW|LOGFILE);
	
	StartDate = STARTDATE;
	EndDate = ENDDATE;
	
	BarPeriod = BARPERIOD;
	LookBack = 100;
	MonteCarlo = 0;
	History = ".t1";	//HISTORY; 
	assetList(ASSETLIST);
	asset(SYMBOL);
	NumCores = -1;
	Outlier = 2;

// set up walk-forward parameters
	int TST = 4*1440/BarPeriod; //number of days in test period
	int TRN = 8*1440/BarPeriod; //number of days in training period
	DataSplit = 100*TRN/(TST+TRN);
	DataHorizon = 1; // 2 bars future peeking
	WFOPeriod = TST+TRN;	
	if(ReTrain) {
		SelectWFO = -1;
	}

// trading cost
	Slippage = 0;
#ifdef ZERO_COST
	Commission = Spread = 0;
#endif

//	data //////////////////////////////////////////////////
	vars Prices = series(priceClose());
	var Objective = sign(priceClose(-DataHorizon)-priceClose(0));	// return sign at 2 bars horizon
	var Features[27]; // Features to be sent to the network
   int M = 0;

// load today's order book ///////////////////////////
	M = orderUpdate(SYMBOL,1);
	T2* Quotes = dataStr(1,OrderRow,0);
	//printf("\nOrderbook: %i quotes",M);
	var Distance = RANGE*price(); // price range
	int N2 = orderCVD(Quotes,M,Distance);
	//printf(", %i in range",N2);

	
// 1. Volume Ask-Bid ratio profile ("orderflow") of the last 2 candles ///////////////////////////////////////
// 	Divides the bid and ask volume profiles into five buckets each holding 20% of the price range 
// 	and calculates the ratio between bid and ask volume for each bucket
	
	int N = 0; // features counter
	
	//PN comment: replaced comment block below with loop
	/*
	Features[N++] = ifelse(cpd(-Prices[0]) 				  == 0, 1., cpd(Prices[0]) 					/ (cpd(Prices[0])               + cpd(-Prices[0])));
	Features[N++] = ifelse(cpd(-Prices[0]-0.25*Distance) == 0, 1., cpd(Prices[0]+0.25*Distance) 	/ (cpd(Prices[0]+0.25*Distance) + cpd(-Prices[0]-0.25*Distance)));
	Features[N++] = ifelse(cpd(-Prices[0]-0.5*Distance)  == 0, 1., cpd(Prices[0]+0.5*Distance) 	/ (cpd(Prices[0]+0.5*Distance)  + cpd(-Prices[0]-0.5*Distance)));
	Features[N++] = ifelse(cpd(-Prices[0]-0.75*Distance) == 0, 1., cpd(Prices[0]+0.75*Distance) 	/ (cpd(Prices[0]+0.75*Distance) + cpd(-Prices[0]-0.75*Distance)));
	Features[N++] = ifelse(cpd( Prices[0]+Distance) + cpd(-Prices[0]-Distance) == 0, 1., (cpd(Prices[0]+Distance) / (cpd(Prices[0]+Distance) + cpd(-Prices[0]-Distance))));
	Features[N++] = ifelse(cpd(-Prices[1]) 				  == 0, 1., cpd(Prices[1])						/ (cpd(Prices[1])               + cpd(-Prices[1])));
	Features[N++] = ifelse(cpd(-Prices[1]-0.25*Distance) == 0, 1., cpd(Prices[1]+0.25*Distance)	/ (cpd(Prices[1]+0.25*Distance) + cpd(-Prices[1]-0.25*Distance)));
	Features[N++] = ifelse(cpd(-Prices[1]-0.5*Distance)  == 0, 1., cpd(Prices[1]+0.5*Distance)   / (cpd(Prices[1]+0.5*Distance)  + cpd(-Prices[1]-0.5*Distance)));
	Features[N++] = ifelse(cpd(-Prices[1]-0.75*Distance) == 0, 1., cpd(Prices[1]+0.75*Distance)  / (cpd(Prices[1]+0.75*Distance) + cpd(-Prices[1]-0.75*Distance)));
	Features[N++] = ifelse(cpd(-Prices[1]-Distance)		  == 0, 1., cpd(Prices[1]+Distance)			/ (cpd(Prices[1]+Distance)      + cpd(-Prices[1]-Distance)));
	*/
	
	var Price, askPriceLevel, bidPriceLevel;
	int b,i;

	//current and prior candle
	for(i=0; i<=1; i++)
	{
		//5 buckets for each price range
		for(b=0; b<5; b++)
		{
			var askPriceLevel = Prices[i] + 0.25*b*Distance;
			var bidPriceLevel = Prices[i] + 0.25*(b+BIDLEVOFFSET)*Distance*BIDDISTMULT;
	//		printf("\n askPrice:%f bidPrice:%f",askPrice,bidPrice);
			var askVol = cpd(askPriceLevel);
			var bidVol = cpd(-bidPriceLevel);
		
			var num = abs(askVol-bidVol);
			var den = abs(askVol+bidVol);
			
			Features[N++] = num / max(den,0.0001);
		}
	}

// 2. Normalized volume profile ("footprint") of the last 2 candles ///////////////////////////////////////
// 	Divides the bid and ask volume profiles into five buckets each holding 20% of the price range 
// 	and calculates the normalized difference between bid and ask volume for each bucket
	
	//PN comment: replaced comment block below with loop
	/*
	Features[N++] = (cpd(Prices[0])                - cpd(-Prices[0]))						/ 100;
	Features[N++] = (cpd(Prices[0]+0.25*Distance)  - cpd(-Prices[0]-0.25*Distance)) 	/ 100;
	Features[N++] = (cpd(Prices[0]+0.5*Distance)   - cpd(-Prices[0]-0.5*Distance))   / 100;
	Features[N++] = (cpd(Prices[0]+0.75*Distance)  - cpd(-Prices[0]-0.75*Distance)) 	/ 100;
	Features[N++] = (cpd(Prices[0]+Distance) 		  - cpd(-Prices[0]-Distance)) 		/ 100;
	Features[N++] = (cpd(Prices[1])               - cpd(-Prices[1])) 						/ 100;	
	Features[N++] = (cpd(Prices[1]+0.25*Distance) - cpd(-Prices[1]-0.25*Distance)) 	/ 100;
	Features[N++] = (cpd(Prices[1]+0.5*Distance)  - cpd(-Prices[1]-0.50*Distance)) 	/ 100;
	Features[N++] = (cpd(Prices[1]+0.75*Distance) - cpd(-Prices[1]-0.75*Distance)) 	/ 100;
	Features[N++] = (cpd(Prices[1]+Distance)      - cpd(-Prices[1]-Distance)) 			/ 100;
	*/
	
	//current and prior candle
	for(i=0; i<=1; i++)
	{
		//5 buckets for each price range
		for(b=0; b<5; b++)
		{
			var askPriceLevel = Prices[i] + 0.25*b*Distance;
			var bidPriceLevel = Prices[i] + 0.25*b*Distance*BIDDISTMULT;
	//		printf("\n askPrice:%f bidPrice:%f",askPrice,bidPrice);
			var askVol = cpd(askPriceLevel);
			var bidVol = cpd(-bidPriceLevel);
			
			Features[N++] = (askVol - bidVol)/100;
		}
	}
	
// 3. Ask-Bid Spread of the last 2 candles, in percent of the price ///////////////////////////////////////
	Features[N++] = marketVal(0)/Prices[0];
	Features[N++] = marketVal(1)/Prices[1];
	
	
// 4. Volatility (High-Low Ranges) of at last 2 candles, in percent of the price ///////////////////////////////////////
	Features[N++] = (priceHigh(0)-priceLow(0))/Prices[0];
	Features[N++] = (priceHigh(1)-priceLow(1))/Prices[1];
	
	
// 5. Ask price change of the last 3 candles, in percent ///////////////////////////////////////
	vars Returns = series((Prices[0]-Prices[1])/Prices[1]);
	Features[N++] = Returns[0];
	Features[N++] = Returns[1];
	Features[N++] = Returns[2];
	
	//	printf(" %.2f %.2f %.2f %.2f %.2f %.2f",
	//		Features[N-1],Features[N-2],Features[N-2],Features[N-4],Features[N-5],Features[N-6]);
	
#ifdef DO_SIGNALS
	SelectWFO = -1; // use the last WFO cycle to store the signals for calibrating the neural net
	var P = adviseLong(SIGNALS|BALANCED,Objective,Features,N);
#else	
	var P = adviseLong(NEURAL|BALANCED,Objective,Features,N);
#endif
	if(!is(TRAINMODE))
	{	
		//printf("\n# => P = %.2f",P);
		if(P > 0.5+FILTER && !NumOpenLong) enterLong();
		if(P < 0.5 && NumOpenLong) exitLong();	
#ifdef SHORTLONG
		if(P < 0.5-FILTER && !NumOpenShort) enterShort();
		if(P > 0.5 && NumOpenShort) exitShort();	
#endif
	}	
}