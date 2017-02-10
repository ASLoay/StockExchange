#include "StockQuoterTypeSupportImpl.h"
#include "QuoteDataReaderListenerImpl.h"
#include "ExchangeEventDataReaderListenerImpl.h"
#include <dds/DCPS/Service_Participant.h>
#include <dds/DCPS/Marked_Default_Qos.h>
#include <dds/DCPS/SubscriberImpl.h>
#include <dds/DCPS/BuiltInTopicUtils.h>
#include <ace/streams.h>
#include "ace/OS_NS_unistd.h"


#include <dds/DdsDcpsInfrastructureC.h>
#include <dds/DdsDcpsSubscriptionC.h>
#include <dds/DCPS/WaitSet.h>

#include "dds/DCPS/StaticIncludes.h"

// constants for Stock Quoter domain Id, types, and topic
// (same as publisher)
DDS::DomainId_t QUOTER_DOMAIN_ID = 1066;
const char* QUOTER_QUOTE_TYPE = "Quote Type";
const char* QUOTER_QUOTE_TOPIC = "Stock Quotes";
const char* QUOTER_EXCHANGE_EVENT_TYPE = "Exchange Event Type";
const char* QUOTER_EXCHANGE_EVENT_TOPIC = "Stock Exchange Events";

int main(int argc, char *argv[]){
	try {
		// Initialize, and create a DomainParticipant
		// (same code as publisher)

		DDS::DomainParticipantFactory_var dpf = TheParticipantFactoryWithArgs(argc, argv);
		DDS::DomainParticipant_var participant = dpf->create_participant(
			QUOTER_DOMAIN_ID,
			PARTICIPANT_QOS_DEFAULT,
			0,
            OpenDDS::DCPS::DEFAULT_STATUS_MASK);

		if (!participant){
			cerr << "create_participant failed." << endl;
			ACE_OS::exit(1);
		}

		// Create a subscriber for the two topics
		// (SUBSCRIBER_QOS_DEFAULT is defined 
		// in Marked_Default_Qos.h)
		DDS::Subscriber_var sub = participant->create_subscriber(SUBSCRIBER_QOS_DEFAULT,
                0,
                OpenDDS::DCPS::DEFAULT_STATUS_MASK);

		if (!sub){
			cerr << "create_subscriber failed." << endl;
			ACE_OS::exit(1);
		}

		// Register the Quote type
		StockQuoter::QuoteTypeSupport_var quote_servant = new StockQuoter::QuoteTypeSupportImpl();

		if (DDS::RETCODE_OK !=quote_servant->register_type(participant,QUOTER_QUOTE_TYPE))
		{
			cerr << "register_type for " << QUOTER_QUOTE_TYPE << " failed." << endl;
			ACE_OS::exit(1);
		}

		// Register the ExchangeEvent type
		StockQuoter::ExchangeEventTypeSupport_var exchange_evt_servant= new StockQuoter::ExchangeEventTypeSupportImpl();

		if (DDS::RETCODE_OK !=exchange_evt_servant->register_type(participant,QUOTER_EXCHANGE_EVENT_TYPE))
		{
			cerr << "register_type for "<< QUOTER_EXCHANGE_EVENT_TYPE<< " failed." << endl;
			ACE_OS::exit(1);
		}

		// Get QoS to use for our two topics
		DDS::TopicQos default_topic_qos;
		participant->get_default_topic_qos(default_topic_qos);

		// Create a topic for the Quote type...
		DDS::Topic_var quote_topic =participant->create_topic(QUOTER_QUOTE_TOPIC,
															QUOTER_QUOTE_TYPE,
															default_topic_qos,
															0,
															OpenDDS::DCPS::DEFAULT_STATUS_MASK);

		if (!quote_topic){
			cerr << "create_topic for " << QUOTER_QUOTE_TOPIC << " failed." << endl;
			ACE_OS::exit(1);
		}

		// .. and another topic for the Exchange Event type
		DDS::Topic_var exchange_evt_topic = participant->create_topic(QUOTER_EXCHANGE_EVENT_TOPIC,
																	QUOTER_EXCHANGE_EVENT_TYPE,
																	default_topic_qos,
																	0,
																	OpenDDS::DCPS::DEFAULT_STATUS_MASK);

		if (!exchange_evt_topic){
			cerr << "create_topic for " << QUOTER_EXCHANGE_EVENT_TOPIC << " failed." << endl;
			ACE_OS::exit(1);
		}

		// Create DataReaders and DataReaderListeners for the
		// Quote and ExchangeEvent

		// Create a Quote listener
		QuoteDataReaderListenerImpl* quote_listener =new QuoteDataReaderListenerImpl();


		if (CORBA::is_nil (quote_listener)){
			cerr << "Quote listener is nil." << endl;
			ACE_OS::exit(1);
		}

		// Create an ExchangeEvent listener

		ExchangeEventDataReaderListenerImpl* exchange_evt_listener=new ExchangeEventDataReaderListenerImpl();

		if (CORBA::is_nil (exchange_evt_listener))
		{
			cerr << "ExchangeEvent listener is nil." << endl;
			ACE_OS::exit(1);
		}

		// Create the Quote DataReader

		// Get the default QoS
		// Could also use DATAREADER_QOS_DEFAULT
		DDS::DataReaderQos dr_default_qos;
		sub->get_default_datareader_qos(dr_default_qos);

		DDS::DataReader_var quote_dr =sub->create_datareader(quote_topic,
															dr_default_qos,
															quote_listener,
															OpenDDS::DCPS::DEFAULT_STATUS_MASK);
		
		// Create the ExchangeEvent DataReader

		DDS::DataReader_var exchange_evt_dr = sub->create_datareader(exchange_evt_topic,
																	dr_default_qos,
																	exchange_evt_listener,
																	OpenDDS::DCPS::DEFAULT_STATUS_MASK);

		// Wait for events from the Publisher; shut
		// down when "close" received
		cout << "Subscriber: waiting for events" << endl;
		// Block until Publisher completes
    while ( ! exchange_evt_listener->is_exchange_closed_received() ) {
      ACE_OS::sleep(1);
    }

    cout << "Received CLOSED event from publisher; exiting..." << endl;


		// Cleanup
		if (participant) {
			  participant->delete_contained_entities();
		}
		if (dpf) {
			  dpf->delete_participant(participant.in());
		}
		TheServiceParticipant->shutdown();
	}catch (CORBA::Exception& e) {
	  cerr << "Exception caught in cleanup."<< endl
		  << e << endl;
	  ACE_OS::exit(1);
	}
	
  return 0;
}