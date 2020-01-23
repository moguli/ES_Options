
#define ASSETLIST			"AssetsCMSW"
#define SYMBOL 			"ESU"
#define HISTORY			".t1"		// historical data file
#define STARTDATE			20190507
#define BARPERIOD			15			//	bar period in minutes
#define RANGE				0.003		// order book range
#define BIDDISTMULT		-1			// usually -1 to get the volume for the bid , +/-1	

function run()
{
	MaxBars = 60;
	StartDate = STARTDATE;
	LookBack = 0;
  	BarPeriod = BARPERIOD;	
	set(PLOTNOW|STEPWISE);
	
	History = HISTORY;
	assetList(ASSETLIST);
	asset(SYMBOL);

	int N1 = orderUpdate(SYMBOL,1);

	T2* Quotes = dataStr(1,OrderRow,0);
  
  	printf("\n N: %i ",N1);
  
	printf("\nOrderbook: %i quotes",N1);
  	var Distance = RANGE*priceClose(); 
  	
  	//Quotes within distance
	int N2 = orderCVD(Quotes,N1,Distance);	
	printf(", %i in range",N2);

	var PriceLevel = priceClose() - Distance/2;

	printf("\n hr:%0.2d,min:%0.2d,date:%i",hour(),minute(),date());
  	
  	int i;
	int increment = 50;
  	for(i = 0; i < increment; i++) 
  	{
    	PriceLevel = PriceLevel + Distance/increment;	
    	printf("\n %i%% from price, \nPrice Level:%.2f Ask volume:%.2f, Bid volume:%.2f",(100*i/increment)-50,PriceLevel,cpd(PriceLevel),cpd(-PriceLevel));

    	plotBar("Ask",i,PriceLevel,cpd(PriceLevel),BARS|LBL2,RED);
    	plotBar("Bid",i,PriceLevel,cpd(-PriceLevel),BARS|LBL2,BLUE);
  	}
  	
  	var Features[5];
  	
	int N = 0; // features counter
	
	//return the results of the first 5 features
	for(N = 0; N < 5; N++)
	{
		var askPriceLevel = priceClose()+0.25*N*Distance;
		
		//The sign 
		var bidPriceLevel = priceClose()+0.25*N*Distance*BIDDISTMULT;
		
		var askVol = cpd(askPriceLevel);
		var bidVol = cpd(-bidPriceLevel);
		
		var num = abs(askVol-bidVol);
		var den = abs(askVol+bidVol);
		
		Features[N] = num / max(den,0.0001);
		
		printf("\n\nFeature: %i, Prices Ask: %.2f, Bid: %.2f",N,askPriceLevel,bidPriceLevel);
		printf("\nVolume Ask: %.2f, Bid: %.2f, numerator:%.2f, denominator:%.2f",askVol,bidVol,num,den);
		printf("\nResult: %.2f",Features[N]);
	}
	
}