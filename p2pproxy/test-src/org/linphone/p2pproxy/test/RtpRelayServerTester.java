package org.linphone.p2pproxy.test;


import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.net.SocketTimeoutException;

import junit.framework.Assert;
import junit.framework.TestCase;

import org.apache.log4j.BasicConfigurator;
import org.junit.After;
import org.junit.Before;
import org.linphone.p2pproxy.core.media.rtprelay.RtpRelayServer;

public class RtpRelayServerTester extends TestCase{

	static private RtpRelayServer mRtpRelayServer;
	static private int RTP_SERVER_PORT = 16000;
	@Before
	public void setUp() throws Exception {
		if (mRtpRelayServer == null) {
		   BasicConfigurator.configure();
		   mRtpRelayServer =  new RtpRelayServer(new InetSocketAddress(RTP_SERVER_PORT),1000,1000); 
		}
	}

	@After
	public void tearDown() throws Exception {
	   //mRtpRelayServer.close();
	}
	public void testRouting() {
	   _testRouting(1);
	}
	private void _testRouting(int aSessionId) {
	   byte lSsrcA = (byte) aSessionId;
	   byte lSsrcB = (byte) ~aSessionId;
	   byte[] lRtcpA = {0x55,(byte) 204,0x0,16
	                     ,0x0,0x01,0x02,lSsrcA //ssrc
				         ,'R','S','I','D'
				         ,'S','E','S','S',(byte) aSessionId};

        byte[] lRtpA = {0x55,(byte) 34,0x0,16
                       ,0x55,0x55,0x55,0x55
                       ,0x0,0x01,0x02,lSsrcA //ssrc
                       ,0x55,0x55,0x55,0x55
                       ,0x55,0x55,0x55,0x55};
		
		
		byte[] lRtcpB = {0x55,(byte) 204,0x0,16
		                ,0x04,0x05,0x06,lSsrcB
		                ,'R','S','I','D'
		                ,'S','E','S','S',(byte) aSessionId};

		byte[] lRtpB = {0x55,(byte) 34,0x0,16
                       ,0x55,0x55,0x55,0x55
                       ,0x04,0x05,0x06,lSsrcB //ssrc
                       ,0x55,0x55,0x55,0x55
                       ,0x55,0x55,0x55,0x55};

		
		try {
			//1 send rtcp app A
			DatagramSocket lRtcpSocketA = new DatagramSocket(new InetSocketAddress("localhost", 0));
			DatagramPacket lRtcpaPacket = new DatagramPacket(lRtcpA,lRtcpA.length,InetAddress.getByName("localhost"), RTP_SERVER_PORT);
			lRtcpSocketA.setSoTimeout(100);
			lRtcpSocketA.send(lRtcpaPacket);
			
			//2 send rtcp app B
			DatagramSocket lRtcpSocketB = new DatagramSocket(new InetSocketAddress("localhost", 0));
			DatagramPacket lRtcpbPacket = new DatagramPacket(lRtcpB,lRtcpB.length,InetAddress.getByName("localhost"), RTP_SERVER_PORT);
			lRtcpSocketB.setSoTimeout(100);
			lRtcpSocketB.send(lRtcpbPacket);
			

            //3 send rtp A
            DatagramSocket lRtpSocketA = new DatagramSocket(new InetSocketAddress("localhost", 0));
            DatagramPacket lRtpaPacket = new DatagramPacket(lRtpA,lRtpA.length,InetAddress.getByName("localhost"), RTP_SERVER_PORT);
            lRtpSocketA.setSoTimeout(100);
            lRtpSocketA.send(lRtpaPacket);
            
            //4 send rtp B
            DatagramSocket lRtpSocketB = new DatagramSocket(new InetSocketAddress("localhost", 0));
            DatagramPacket lRtpblPacket = new DatagramPacket(lRtpB,lRtpB.length,InetAddress.getByName("localhost"), RTP_SERVER_PORT);
            lRtpSocketB.send(lRtpblPacket);
            lRtpSocketB.setSoTimeout(100);
            
            
            
            // check RTP B -> A
            DatagramPacket lReceivedRtpaPacket = new DatagramPacket(new byte[1500],1500);
            
          
            try {
               lRtpSocketA.receive(lReceivedRtpaPacket);
               // check ssrc
               Assert.assertEquals("Unexpected packet received ",lSsrcB,(byte)RtpRelayServer.b2UB(lReceivedRtpaPacket.getData()[11]));
            } catch (SocketTimeoutException e) {
               Assert.fail("packet not relayed cause [" + e.getMessage()+"]");
            }

            //check RTP A->B
            DatagramPacket lReceivedRtpbPacket = new DatagramPacket(new byte[1500],1500);
            lRtpSocketA.setSoTimeout(100);
            lRtpSocketA.send(lRtpaPacket);
            try {
               lRtpSocketB.receive(lReceivedRtpbPacket);
               // check ssrc
               Assert.assertEquals("Unexpected packet received ",lSsrcA,(byte)RtpRelayServer.b2UB(lReceivedRtpbPacket.getData()[11]));
            } catch (SocketTimeoutException e) {
               Assert.fail("packet not relayed cause [" + e.getMessage()+"]");
            }
            
            // check RTCP A->B
            byte[] lRtcpASR = {0x55,(byte) 200,0x0,16
                              ,0x0,0x01,0x02,lSsrcA //ssrc
                              ,'B','L','A','B','L','A'};
            
            DatagramPacket lRtcpASRPacket = new DatagramPacket(lRtcpASR,lRtcpASR.length,InetAddress.getByName("localhost"), RTP_SERVER_PORT);
            lRtcpSocketA.send(lRtcpASRPacket);
            try {
               lRtcpSocketB.receive(lReceivedRtpbPacket);
               // check ssrc
               Assert.assertEquals("Unexpected packet received ",lSsrcA,(byte)RtpRelayServer.b2UB(lReceivedRtpbPacket.getData()[7]));
            } catch (SocketTimeoutException e) {
               Assert.fail("packet not relayed cause [" + e.getMessage()+"]");
            }
            
            // check RTCP B->A
            byte[] lRtcpBSR = {0x55,(byte) 200,0x0,16
                              ,0x04,0x05,0x06,lSsrcB //ssrc
                              ,'B','L','A','B','L','A'};
            
            DatagramPacket lRtcpBSRPacket = new DatagramPacket(lRtcpBSR,lRtcpBSR.length,InetAddress.getByName("localhost"), RTP_SERVER_PORT);
            lRtcpSocketB.send(lRtcpBSRPacket);
            try {
               lRtcpSocketA.receive(lReceivedRtpbPacket);
               // check ssrc
               Assert.assertEquals("Unexpected packet received ",lSsrcB,(byte)RtpRelayServer.b2UB(lReceivedRtpbPacket.getData()[7]));
            } catch (SocketTimeoutException e) {
               Assert.fail("packet not relayed cause [" + e.getMessage()+"]");
            }

            // check unknown RTCP B->A
            byte[] lRtcpSR = {0x55,(byte) 200,0x0,16
                              ,0x55,0x55,0x56,0x55 //ssrc
                              ,'B','L','A','B','L','A'};
            
            DatagramPacket lRtcpSRPacket = new DatagramPacket(lRtcpSR,lRtcpSR.length,InetAddress.getByName("localhost"), RTP_SERVER_PORT);
            lRtcpSocketB.send(lRtcpSRPacket);
            try {
               lRtcpSocketA.receive(lReceivedRtpbPacket);
               // check ssrc
               Assert.fail("Unexpected packet received ");
            } catch (SocketTimeoutException e) {
               //ok
            }

            
		} catch (Exception e) {
			Assert.fail(e.getMessage());
		}

	}
	public void testGC() {
	   try {
      //1 launch traffic
	      int lOldValue = mRtpRelayServer.getRoutingtableSize();
	   _testRouting(2);
	   Assert.assertEquals("unexpected routing table size", lOldValue+1,mRtpRelayServer.getRoutingtableSize());
	   //2 wait 1,5s
	   Thread.sleep(1500);
	   Assert.assertTrue("unexpected routing table size ["+mRtpRelayServer.getRoutingtableSize()+"] should be less than or equal to ["+lOldValue+"]", mRtpRelayServer.getRoutingtableSize()<=lOldValue);
	   } catch (Exception e) {
          Assert.fail(e.getMessage());
      }
	   
	}

}
