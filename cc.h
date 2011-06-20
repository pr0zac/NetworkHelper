#ifndef CCH
#define CCH

#include <udt.h>
#include <ccc.h>

class CUDPBlast: public CCC
{
public:
   CUDPBlast()
   {
	   //m_iMSS = 900000;
	   m_dPktSndPeriod = 0;//(m_iMSS * 8.0)/INT32_MAX;
	   m_dCWndSize = 83333000;
   }

public:
   void setRate(double mbps)
   {
      m_dPktSndPeriod = (m_iMSS * 8.0) / mbps;
   }
};

class ZUDTCC: public CCC
{
public:
	ZUDTCC();
	
public:
	virtual void init();
	virtual void onACK(const int32_t&);
	virtual void onLoss(const int32_t*, const int&);
	virtual void onTimeout();
	void setBW(int64_t bw);
	
private:
	int m_iRCInterval;			// UDT Rate control interval
	uint64_t m_LastRCTime;		// last rate increase time
	bool m_bSlowStart;			// if in slow start phase
	int32_t m_iLastAck;			// last ACKed seq no
	bool m_bLoss;			// if loss happened since last rate increase
	int32_t m_iLastDecSeq;		// max pkt seq no sent out when last decrease happened
	double m_dLastDecPeriod;		// value of pktsndperiod when last decrease happened
	int m_iNAKCount;                     // NAK counter
	int m_iDecRandom;                    // random threshold on decrease by number of loss events
	int m_iAvgNAKNum;                    // average number of NAKs per congestion
	int m_iDecCount;			// number of decreases in a congestion epoch
	
	double m_dCWndModifier;		// Modifier for window size to increase aggressiveness
};

#endif
