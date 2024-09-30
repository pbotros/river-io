/*
    ------------------------------------------------------------------

    This file is part of the Open Ephys GUI
    Copyright (C) 2016 Open Ephys

    ------------------------------------------------------------------

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#ifndef __RIVEROUTPUT_H_F7BDA585__
#define __RIVEROUTPUT_H_F7BDA585__

#include <ProcessorHeaders.h>
#include <river/river.h>

typedef struct {
    std::vector<char> rawData;
    int numSamples;
} QueuedEvent;

/** 

    Writes data to the Redis database inside a thread

*/
class RiverWriterThread : public Thread 
{
public:

    /** Constructor */
    RiverWriterThread(river::StreamWriter *writer, int batch_period_ms);

	/** Destructor */
    ~RiverWriterThread() override = default;

    /** Run thread */
    void run() override;

    /** Adds bytes to the writing queue */
    void enqueue(const QueuedEvent& event);

private:
    std::queue<QueuedEvent> queued_events_;
    std::mutex queue_mutex_;

    int batch_period_ms_;

    river::StreamWriter* writer_;
};

/**
 *  A sink that writes spikes and events to a Redis database,
 *  using the River library.
 * 
 * 
    @see GenericProcessor
 */
class RiverOutput : public GenericProcessor
{
public:
    /** Constructor */
    RiverOutput();

    /** Destructor */
    virtual ~RiverOutput();

    /** Called when a processor needs to update its settings */
    void updateSettings() override;

    /** Test connection to Redis database*/
    bool testConnection();

    /** Searches for events and triggers the River output when appropriate. */
    void process(AudioSampleBuffer &buffer) override;

    /** Copies a spike into the output buffer */
    void handleSpike(SpikePtr spike) override;
   
    /** Copies a TTL event into the output buffer*/
    void handleTTLEvent(TTLEventPtr event) override;

    /** Called immediately prior to the start of data acquisition. */
    bool startAcquisition() override;

    /** Called immediately after the end of data acquisition. */
    bool stopAcquisition() override;

    /** Creates the RiverOutputEditor. */
    AudioProcessorEditor *createEditor() override;

    /** Convert parameters to XML */
    void saveCustomParametersToXml (XmlElement* xml) override;
    
    /** Load custom parameters from XML*/
    void loadCustomParametersFromXml(XmlElement* xml) override;

    /** Called when a parameter is updated*/
    void parameterValueChanged(Parameter* param) override;
    void registerParameters() override;

    //
    // Non-override methods:
    //
    std::string redisConnectionHostname();
    void setRedisConnectionHostname(const std::string &redisConnectionHostname);
    int redisConnectionPort();
    void setRedisConnectionPort(int redisConnectionPort);
    std::string redisConnectionPassword();
    void setRedisConnectionPassword(const std::string &redisConnectionPassword);

    void setEventSchema(const river::StreamSchema& eventSchema);
    void clearEventSchema();
    bool shouldConsumeSpikes() const;

    river::StreamSchema getSchema() const;

    std::string streamName();
    int64_t totalSamplesWritten() const;

    int maxBatchSize() {
        return getParameter("max_batch_size")->getValue();
    }

    int maxLatencyMs() {
        return getParameter("max_latency_ms")->getValue();
    }

    int datastream_id();

    void setMaxBatchSize(int maxBatchSize) {
        return getParameter("max_batch_size")->setNextValue(maxBatchSize);
    }

    void setDatastreamId(uint16 datastream_id) {
        getParameter("datastream_id")->setNextValue(datastream_id);
    }

    void setStreamName(const std::string &stream_name) {
        getParameter("stream_name")->setNextValue(juce::String(stream_name));
    }


    void setMaxLatencyMs(int maxLatencyMs) {
        return getParameter("max_latency_ms")->setNextValue(maxLatencyMs);
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RiverOutput)

    // For writing spikes to River. Ensure it's packed so that padding doesn't mess up the size of the struct.
    typedef struct {
        int32_t channel_index;
        int32_t unit_index;
        int64_t sample_number;
    } __attribute__((__packed__)) RiverSpike;

    const river::StreamSchema spike_schema_;

    // If this is set, then we should listen to events, not spikes.
    std::shared_ptr<river::StreamSchema> event_schema_;

    std::unique_ptr<river::StreamWriter> writer_;
    std::unique_ptr<RiverWriterThread> writing_thread_;

    std::unordered_map<int, std::string> stream_id_to_stream_names;
};


#endif  // __RIVEROUTPUT_H_F7BDA585__
