#include "StockQuoterTypeSupportImpl.h"
#include <dds/DCPS/Service_Participant.h>
#include <dds/DCPS/Marked_Default_Qos.h>
#include <dds/DCPS/PublisherImpl.h>
#include <ace/streams.h>
#include <ace/OS_NS_unistd.h>
#include <orbsvcs/Time_Utilities.h>
#include <dds/DCPS/WaitSet.h>
#include "dds/DCPS/StaticIncludes.h"

// constants for Stock Quoter domain Id, types, and topic
DDS::DomainId_t QUOTER_DOMAIN_ID = 1066;
const char* QUOTER_QUOTE_TYPE = "Quote Type";
const char* QUOTER_QUOTE_TOPIC = "Stock Quotes";
const char* QUOTER_EXCHANGE_EVENT_TYPE = "Exchange Event Type";
const char* QUOTER_EXCHANGE_EVENT_TOPIC = "Stock Exchange Events";

const char* STOCK_EXCHANGE_NAME = "Test Stock Exchange";

TimeBase::TimeT get_timestamp(){
	TimeBase::TimeT retval;
	ACE_hrtime_t t = ACE_OS::gethrtime();
	ORBSVCS_Time::hrtime_to_TimeT(retval, t);
	return retval;
}

int main(int argc, char *argv[]){
	try{
		// Initialize, and create a DomainParticipant
		DDS::DomainParticipantFactory_var dpf = TheParticipantFactoryWithArgs(argc, argv);
		DDS::DomainParticipant_var participant = dpf->create_participant(
			QUOTER_DOMAIN_ID,
			PARTICIPANT_QOS_DEFAULT,
			0,
            OpenDDS::DCPS::DEFAULT_STATUS_MASK);

		if (!participant){
			ACE_ERROR_RETURN((LM_ERROR,
                        ACE_TEXT("ERROR: %N:%l: main() -")
                        ACE_TEXT(" create_participant failed!\n")),
                       -1);
		}

		// Create a publisher for the two topics
		DDS::Publisher_var pub = participant->create_publisher(
				PUBLISHER_QOS_DEFAULT,
                0,
                OpenDDS::DCPS::DEFAULT_STATUS_MASK);

		if (!pub){
			ACE_ERROR_RETURN((LM_ERROR,
                        ACE_TEXT("ERROR: %N:%l: main() -")
                        ACE_TEXT(" create_publisher failed!\n")),
                       -1);
		}

		// Register the Quote type
		StockQuoter::QuoteTypeSupport_var quote_servant = new StockQuoter::QuoteTypeSupportImpl();

		if (quote_servant->register_type(participant,QUOTER_QUOTE_TYPE) != DDS::RETCODE_OK ){
			cerr << "register_type for " << QUOTER_QUOTE_TYPE << " failed." << endl;
			ACE_OS::exit(1);
		}

		// Register the ExchangeEvent type
		StockQuoter::ExchangeEventTypeSupport_var exchange_evt_servant = new StockQuoter::ExchangeEventTypeSupportImpl();

		if (exchange_evt_servant->register_type(participant, QUOTER_EXCHANGE_EVENT_TYPE) != DDS::RETCODE_OK){
			cerr << "register_type for " << QUOTER_EXCHANGE_EVENT_TYPE << " failed." << endl;
			ACE_OS::exit(1);
		}

		// Get QoS to use for our two topics
		// Could also use TOPIC_QOS_DEFAULT instead
		DDS::TopicQos default_topic_qos;
		participant->get_default_topic_qos(default_topic_qos);

		// Create a topic for the Quote type...
		DDS::Topic_var quote_topic = participant->create_topic(QUOTER_QUOTE_TOPIC,
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

		// Get QoS to use for our two DataWriters
		// Could also use DATAWRITER_QOS_DEFAULT
		DDS::DataWriterQos dw_default_qos;
		pub->get_default_datawriter_qos(dw_default_qos);

		// Create a DataWriter for the Quote topic
		DDS::DataWriter_var quote_base_dw = pub->create_datawriter(quote_topic,
																dw_default_qos,
																0,
																OpenDDS::DCPS::DEFAULT_STATUS_MASK);

		if (!quote_base_dw)
		{
			cerr << "create_datawriter for "
				<< QUOTER_QUOTE_TOPIC
				<< " failed." << endl;
			ACE_OS::exit(1);
		}

		StockQuoter::QuoteDataWriter_var quote_dw = StockQuoter::QuoteDataWriter::_narrow(quote_base_dw);

		if (!quote_dw){
			cerr << "QuoteDataWriter could not be narrowed"<< endl;
			ACE_OS::exit(1);
		}

		// Create a DataWriter for the Exchange Event topic
		DDS::DataWriter_var exchange_evt_base_dw = pub->create_datawriter(exchange_evt_topic,
																		dw_default_qos,
																		0,
																		OpenDDS::DCPS::DEFAULT_STATUS_MASK);

		if (!exchange_evt_base_dw)
		{
			cerr << "create_datawriter for " << QUOTER_EXCHANGE_EVENT_TOPIC << " failed." << endl;
			ACE_OS::exit(1);
		}

		StockQuoter::ExchangeEventDataWriter_var exchange_evt_dw = StockQuoter::ExchangeEventDataWriter::_narrow(exchange_evt_base_dw);

		if (!exchange_evt_dw){
			cerr << "ExchangeEventDataWriter could not " << "be narrowed" << endl;
			ACE_OS::exit(1);
		}

		// Register the Exchange Event and the two
		// Quoted securities (SPY and MDY) with the
		// appropriate data writer

		StockQuoter::Quote spy;
		spy.ticker = "SPY";
		DDS::InstanceHandle_t spy_handle = quote_dw->register_instance(spy);

		StockQuoter::Quote mdy;
		mdy.ticker = "MDY";
		DDS::InstanceHandle_t mdy_handle = quote_dw->register_instance(mdy);

		StockQuoter::ExchangeEvent ex_evt;
		ex_evt.exchange = STOCK_EXCHANGE_NAME;
		DDS::InstanceHandle_t exchange_handle = exchange_evt_dw->register_instance(ex_evt);

		// Block until Subscriber is available
		DDS::StatusCondition_var condition = exchange_evt_dw->get_statuscondition();
		condition->set_enabled_statuses(DDS::PUBLICATION_MATCHED_STATUS);

		DDS::WaitSet_var ws = new DDS::WaitSet;
		ws->attach_condition(condition);

		while (true) {
		  DDS::PublicationMatchedStatus matches;
		  if (exchange_evt_dw->get_publication_matched_status(matches) != ::DDS::RETCODE_OK) {
			ACE_ERROR_RETURN((LM_ERROR,
							  ACE_TEXT("ERROR: %N:%l: main() -")
							  ACE_TEXT(" get_publication_matched_status failed!\n")),
							 -1);
		  }

		  if (matches.current_count >= 1) {
			break;
		  }

		  DDS::ConditionSeq conditions;
		  DDS::Duration_t timeout = { 60, 0 };
		  if (ws->wait(conditions, timeout) != DDS::RETCODE_OK) {
			ACE_ERROR_RETURN((LM_ERROR,
							  ACE_TEXT("ERROR: %N:%l: main() -")
							  ACE_TEXT(" wait failed!\n")),
							 -1);
		  }
		}

		ws->detach_condition(condition);
		// Publish...

		StockQuoter::ExchangeEvent opened;
		opened.exchange = STOCK_EXCHANGE_NAME;
		opened.event = StockQuoter::TRADING_OPENED;
		opened.timestamp = get_timestamp();

		cout << "Publishing TRADING_OPENED" << endl;
		DDS::ReturnCode_t ret =
			exchange_evt_dw->write(opened, exchange_handle);

		if (ret != DDS::RETCODE_OK)
		{
			ACE_ERROR((LM_ERROR,ACE_TEXT("(%P|%t)ERROR: OPEN write returned %d.\n"),ret));
		}

		ACE_Time_Value quarterSecond(0, 250000);

		for (int i = 0; i < 20; ++i){
			
			//
			// SPY
			//

			StockQuoter::Quote spy_quote;
			spy_quote.exchange = STOCK_EXCHANGE_NAME;
			spy_quote.ticker = "SPY";
			spy_quote.full_name = "S&P Depository Receipts";
			spy_quote.value = 1200.0 + 10.0*i;
			spy_quote.timestamp = get_timestamp();

			cout << "Publishing SPY Quote: " << spy_quote.value << endl;

			ret = quote_dw->write(spy_quote, spy_handle);

			if (ret != DDS::RETCODE_OK){
				ACE_ERROR(( LM_ERROR, ACE_TEXT("(%P|%t)ERROR: SPY write returned %d.\n"), ret));
			}

			ACE_OS::sleep(quarterSecond);

			//
			// MDY
			//

			StockQuoter::Quote mdy_quote;
			mdy_quote.exchange = STOCK_EXCHANGE_NAME;
			mdy_quote.ticker = "MDY";
			mdy_quote.full_name = "S&P Midcap Depository Receipts";
			mdy_quote.value = 1400.0 + 10.0*i;
			mdy_quote.timestamp = get_timestamp();

			cout << "Publishing MDY Quote: " << mdy_quote.value << endl;

			ret = quote_dw->write(mdy_quote, mdy_handle);

			if (ret != DDS::RETCODE_OK){
				ACE_ERROR((LM_ERROR, ACE_TEXT("(%P|%t)ERROR: MDY write returned %d.\n"), ret));
			}

			ACE_OS::sleep(quarterSecond);
		}

		StockQuoter::ExchangeEvent closed;
		closed.exchange = STOCK_EXCHANGE_NAME;
		closed.event = StockQuoter::TRADING_CLOSED;
		closed.timestamp = get_timestamp();

		cout << "Publishing TRADING_CLOSED" << endl;

		ret = exchange_evt_dw->write(closed, exchange_handle);

		if (ret != DDS::RETCODE_OK){
			ACE_ERROR((LM_ERROR,ACE_TEXT("(%P|%t)ERROR: CLOSED write returned %d.\n"),ret));
		}

		cout << "Exiting..." << endl;
		 // Cleanup
		if (participant) {
			participant->delete_contained_entities();
		}
		if (dpf) {
			dpf->delete_participant(participant);
		}
		  
		  
		  TheServiceParticipant->shutdown();
  }
  catch (CORBA::Exception& e) {
	  cerr << "Exception caught in main.cpp:" << endl 
		  << e << endl;
	  ACE_OS::exit(1);
  }
  return 0;
}