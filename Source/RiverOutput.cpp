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

#include "RiverOutput.h"
#include "RiverOutputEditor.h"
#include <memory>
#include <unordered_map>
#include <chrono>
#include <thread>

RiverOutput::RiverOutput()
        : GenericProcessor("River Output"),
          spike_schema_({river::FieldDefinition("channel_index", river::FieldDefinition::INT32, 4),
                         river::FieldDefinition("unit_index", river::FieldDefinition::INT32, 4),
                         river::FieldDefinition("sample_number", river::FieldDefinition::INT64, 8)
                        }) {
    addStringParameter(
            Parameter::ParameterScope::GLOBAL_SCOPE,
            "stream_name",
            "Stream Name",
            "River stream name",
            "",
            true);
    addStringParameter(
            Parameter::ParameterScope::GLOBAL_SCOPE,
            "redis_connection_hostname",
            "Redis Hostname",
            "Hostname, Redis connection",
            "127.0.0.1",
            true);
    addStringParameter(
            Parameter::ParameterScope::GLOBAL_SCOPE,
            "redis_connection_password",
            "Redis Password",
            "Password, Redis connection",
            "",
            true);
    addIntParameter(
            Parameter::ParameterScope::GLOBAL_SCOPE,
            "redis_connection_port",
            "Redis Port",
            "Hostname, Redis port",
            6379,
            0,
            65535,
            true);
    addIntParameter(
            Parameter::ParameterScope::GLOBAL_SCOPE,
            "max_latency_ms",
            "Max Latency (ms)",
            "Max latency for sending each batch (in ms)",
            5,
            0,
            1000,
            true);
    addIntParameter(
            Parameter::ParameterScope::GLOBAL_SCOPE,
            "datastream_id",
            "Datastream ID",
            "ID of the datastream to listen to events on",
            0,
            0,
            (std::numeric_limits<int32_t>::max)(),
            true);
}

RiverOutput::~RiverOutput()
{
    if (writer_) {
        writer_->Stop();
    }
}

bool RiverOutput::testConnection()
{
    river::RedisConnection connection(
        redisConnectionHostname(),
        redisConnectionPort(),
        redisConnectionPassword(),
        // Wait a max of 5 seconds for testing connection.
        5);

    try {
        river::StreamWriter writer(connection);
        return true;
        // TODO: write exception here:
    } catch (const std::exception& e) {
        LOGC("Failed to connect to Redis: ", e.what());
        CoreServices::sendStatusMessage("Failed to connect to Redis database.");
    }

    return false;

}

/** Called when a processor needs to update its settings */
void RiverOutput::updateSettings()
{
    LOGD("Testing connection to Redis database...");
    isEnabled = testConnection() && !getDataStreams().isEmpty();
    if (isEnabled) {
        LOGC("Connection to Redis database successful.");
        CoreServices::sendStatusMessage("Connection to Redis database successful.");
    } else {
        LOGC("Connection to Redis database failed.");
        CoreServices::sendStatusMessage("Connection to Redis database failed.");
    }

    // Is our currently selected datastream ID in our list?
    auto currently_selected_stream_id = datastream_id();

    uint16 combobox_id = 0;
    for (const auto &item: getDataStreams()) {
        auto stream_id = item->getStreamId();
        if (stream_id == currently_selected_stream_id) {
            combobox_id = stream_id;
            break;
        }
    }

    setDatastreamId(combobox_id);
    ((RiverOutputEditor *) editor.get())->refreshDatastreams(getDataStreams());

    stream_id_to_stream_names.clear();
    for (const auto &item: getDataStreams()) {
        stream_id_to_stream_names[item->getStreamId()] = item->getName().toStdString();
    }
}


AudioProcessorEditor *RiverOutput::createEditor() {
    editor = std::make_unique<RiverOutputEditor>(this);
    return editor.get();
}


void RiverOutput::handleSpike(SpikePtr spike)
{

    RiverSpike river_spike;
    int channel_index = spike->getChannelIndex();

    river_spike.channel_index = channel_index;
    river_spike.sample_number = spike->getSampleNumber();

    // TODO: 0-index option for unit index
    river_spike.unit_index = spike->getSortedId();

    if (writing_thread_) {
        QueuedEvent event;
        event.rawData.resize(sizeof(RiverSpike));
        memcpy(event.rawData.data(), &river_spike, sizeof(RiverSpike));
        event.numSamples = 1;
        writing_thread_->enqueue(event);
    } else {
        writer_->WriteBytes(reinterpret_cast<const char *>(&river_spike), 1);
    }
}

void RiverOutput::handleTTLEvent(TTLEventPtr event) {
    int stream_id = datastream_id();
    auto stream_name = stream_id_to_stream_names[stream_id];
    if (event->getStreamId() != stream_id) {
        return;
    }

    if (event->getMetadataValueCount() != 1) {
        LOGD("Ignoring event received in RiverOutput since invalid number of metadata values found.");
        return;
    }
    auto event_metadata_size = event->getChannelInfo()->getTotalEventMetadataSize();
    if (event_metadata_size == 0) {
        LOGD("Ignoring event received in RiverOutput since metadata was zero sized.");
        return;
    }
    if (event_metadata_size % event_schema_->sample_size() != 0) {
        LOGD("Ignoring event received in RiverOutput since event metadata size did not evenly divide schema size.");
        return;
    }

    LOGD("Processing TTL for event at sample", event->getSampleNumber());

    // Assume that the binary data in the event matches the sample size exactly. If it doesn't, crashes will happen!
    int num_samples = (int) (event_metadata_size / event_schema_->sample_size());
    auto ptr = event->getMetadataValue(0)->getRawValuePointer();

    if (writing_thread_) {
        QueuedEvent queuedEvent;
        queuedEvent.rawData.resize(event_metadata_size);
        memcpy(queuedEvent.rawData.data(), ptr, event_metadata_size);
        queuedEvent.numSamples = num_samples;
        writing_thread_->enqueue(queuedEvent);
    } else {
        writer_->WriteBytes(reinterpret_cast<const char *>(ptr), num_samples);
    }
}

bool RiverOutput::startAcquisition()
{
    auto sn = streamName();
    if (sn.empty() || redisConnectionHostname().empty() || redisConnectionPort() <= 0) {
        CoreServices::sendStatusMessage("FAILED TO ENABLE");
        return false;
    }

    if (writing_thread_) {
        // This shouldn't really happen since any threads should've been stopped in stopAcquisition()... but handle
        // it anyways.
        jassertfalse;
        writing_thread_->stopThread(1000 + maxLatencyMs());
        writing_thread_.reset();
    }
    if (writer_) {
        // Should already be stopped, but no effect if a stopped writer is stopped again.
        writer_->Stop();
        writer_.reset();
    }

    river::RedisConnection connection(
            redisConnectionHostname(),
            redisConnectionPort(),
            redisConnectionPassword(),
            // TODO: allow for configurable timeout
            5);

      LOGD("River Output Connection: ", redisConnectionHostname(), ":", redisConnectionPort());

      try {
          writer_ = std::make_unique<river::StreamWriter>(connection);
      } catch (const std::exception& e) {
          LOGC("Failed to connect to Redis: ", e.what());
          CoreServices::sendStatusMessage("Failed to connect to Redis.");
          CoreServices::setAcquisitionStatus(false);
          isEnabled = false;
          CoreServices::updateSignalChain(this->getEditor());
          return false;
      }

      LOGD("Created StreamWriter.");

      std::unordered_map<std::string, std::string> metadata;

      if (shouldConsumeSpikes())
      {
          if (spikeChannels.size() == 0) {
              // Can't consume spikes if there are no spike channels.
              CoreServices::sendStatusMessage("River Output has no spike channels.");
              return false;
          }

          // Assume that all spike channels have the same details.
          auto spike_channel = getSpikeChannel(0);
          metadata["prepeak_samples"] = std::to_string(spike_channel->getPrePeakSamples());
          metadata["postpeak_samples"] = std::to_string(spike_channel->getPostPeakSamples());
          metadata["sampling_rate"] = std::to_string(CoreServices::getGlobalSampleRate());
      }

     LOGD("Initialized StreamWriter.");
     writer_->Initialize(sn, getSchema(), metadata);

    if (editor) {
        // GenericEditor#enable isn't marked as virtual, so need to *upcast* to VisualizerEditor :(
        ((VisualizerEditor *) (editor.get()))->enable();
    }

    // If latency or batch size are nonpositive, write everything synchronously.
    if (maxLatencyMs() > 0) {
        writing_thread_ = std::make_unique<RiverWriterThread>(writer_.get(), maxLatencyMs());
        writing_thread_->startThread();
        LOGC("Writing to River asynchronously with stream name ", sn);
    } else {
        LOGC("Writing to River synchronously with stream name ", sn);
    }

    return true;
}


bool RiverOutput::stopAcquisition()
{
    if (writing_thread_) {
        writing_thread_->stopThread(1000 + maxLatencyMs());
        writing_thread_.reset();
    }

    if (writer_) {
        writer_->Stop();
        // Don't clear the writer just yet so that totalSamplesWritten() (and maybe
        // other methods) stay valid.
    }

    if (editor) {
        // GenericEditor#enable isn't marked as virtual, so need to *upcast* to VisualizerEditor :(
        ((VisualizerEditor *) (editor.get()))->disable();
    }

    return true;
}


void RiverOutput::process(AudioSampleBuffer &buffer)
{
    if (writer_) {
        checkForEvents(shouldConsumeSpikes());
    }
}

std::string RiverOutput::streamName() {
    return getParameter("stream_name")->getValueAsString().toStdString();
}

int RiverOutput::datastream_id() {
    return getParameter("datastream_id")->getValue();
}

int64_t RiverOutput::totalSamplesWritten() const {
    if (writer_) {
        return writer_->total_samples_written();
    } else {
        return 0;
    }
}

std::string RiverOutput::redisConnectionHostname() {
    return getParameter("redis_connection_hostname")->getValueAsString().toStdString();
}

void RiverOutput::setRedisConnectionHostname(const std::string &redisConnectionHostname) {
    getParameter("redis_connection_hostname")->setNextValue(juce::String(redisConnectionHostname));
}

int RiverOutput::redisConnectionPort() {
    return getParameter("redis_connection_port")->getValue();
}

void RiverOutput::setRedisConnectionPort(int redisConnectionPort) {
    getParameter("redis_connection_port")->setNextValue(redisConnectionPort);
}

std::string RiverOutput::redisConnectionPassword() {
    return getParameter("redis_connection_password")->getValueAsString().toStdString();
}

void RiverOutput::setRedisConnectionPassword(const std::string &redisConnectionPassword) {
    getParameter("redis_connection_password")->setNextValue(juce::String(redisConnectionPassword));
}

void RiverOutput::saveCustomParametersToXml(XmlElement *parentElement) {
    XmlElement *mainNode = parentElement->createNewChildElement("RiverOutput");
    mainNode->setAttribute("hostname", redisConnectionHostname());
    mainNode->setAttribute("port", redisConnectionPort());
    mainNode->setAttribute("password", redisConnectionPassword());
    mainNode->setAttribute("max_latency_ms", maxLatencyMs());
    mainNode->setAttribute("stream_name", streamName());
    mainNode->setAttribute("datastream_id", datastream_id());

    if (event_schema_) {
        std::string event_schema_json = event_schema_->ToJson();
        mainNode->setAttribute("event_schema_json", event_schema_json);
    }
}

void RiverOutput::loadCustomParametersFromXml(XmlElement* xml) {
    forEachXmlChildElement(*xml, mainNode) {
        if (!mainNode->hasTagName("RiverOutput")) {
            continue;
        }

        setRedisConnectionHostname(mainNode->getStringAttribute("hostname", "127.0.0.1").toStdString());
        setRedisConnectionPort(mainNode->getIntAttribute("port", 6379));
        setRedisConnectionPassword(mainNode->getStringAttribute("password", "").toStdString());

        if (mainNode->hasAttribute("max_latency_ms")) {
            setMaxLatencyMs(mainNode->getIntAttribute("max_latency_ms"));
        }
        if (mainNode->hasAttribute("stream_name")) {
            setStreamName(mainNode->getStringAttribute("stream_name").toStdString());
        }
        if (mainNode->hasAttribute("datastream_id")) {
            setDatastreamId(mainNode->getIntAttribute("datastream_id"));
        }
        if (mainNode->hasAttribute("event_schema_json")) {
            String s = mainNode->getStringAttribute("event_schema_json");
            std::string j = s.toStdString();
            try {
                const river::StreamSchema& schema = river::StreamSchema::FromJson(j);
                setEventSchema(schema);
            } catch (const std::exception& e) {
                LOGC("Invalid schema json: ", j, " | ", e.what());
                clearEventSchema();
            }
        } else {
            clearEventSchema();
        }
    }

    ((RiverOutputEditor *) editor.get())->refreshSchemaFromProcessor();
    ((RiverOutputEditor *) editor.get())->refreshLabelsFromProcessor();
    ((RiverOutputEditor *) editor.get())->updateProcessorSchema();
}

void RiverOutput::setEventSchema(const river::StreamSchema& eventSchema) {
    auto p = std::make_shared<river::StreamSchema>(eventSchema);
    event_schema_.swap(p);
    ((RiverOutputEditor *) editor.get())->refreshSchemaFromProcessor();
}

void RiverOutput::clearEventSchema() {
    event_schema_.reset();
    ((RiverOutputEditor *) editor.get())->refreshSchemaFromProcessor();
}

bool RiverOutput::shouldConsumeSpikes() const {
    return !event_schema_;
}

river::StreamSchema RiverOutput::getSchema() const {
    if (event_schema_) {
        return *event_schema_;
    }
    return spike_schema_;
}

/** Called when a parameter is updated*/
void RiverOutput::parameterValueChanged(Parameter* param) {
    if (editor) {
        const MessageManagerLock mm;
        ((RiverOutputEditor *) editor.get())->refreshLabelsFromProcessor();
    }
}

RiverWriterThread::RiverWriterThread(river::StreamWriter *writer, int batch_period_ms)
        : juce::Thread("RiverWriter") {
    writer_ = writer;
    batch_period_ms_ = batch_period_ms;
}

void RiverWriterThread::run() {
    while (!threadShouldExit()) {
        auto start = std::chrono::high_resolution_clock::now();

        // Send all events that are queued up
        while (true) {
            QueuedEvent event;

            // Only hold the lock while manipulating the queue, but not while sending via River
            {
                const std::lock_guard<std::mutex> lock(queue_mutex_);
                if (queued_events_.empty()) {
                    break;
                }
                event = queued_events_.front();
                queued_events_.pop();
            }
            writer_->WriteBytes(event.rawData.data(), event.numSamples);
        }

        // Check again pre-emptively so we can bail before sleeping
        if (threadShouldExit()) {
            break;
        }

        // TODO: should be able to wake up here if someone clicks "stop" while this is sleeping
        std::this_thread::sleep_until(start + std::chrono::milliseconds(batch_period_ms_));
    }
}

void RiverWriterThread::enqueue(const QueuedEvent& event) {
    if (event.numSamples == 0) {
        return;
    }

    const std::lock_guard<std::mutex> lock(queue_mutex_);
    queued_events_.push(event);
}
