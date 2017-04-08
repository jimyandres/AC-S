//#ifndef PLAYER_H
//#define PLAYER_H

#include <SFML/Audio.hpp>
#include <vector>
#include <string.h>
#include <stdio.h>
#include <iostream>

#define CHUNK_SIZE 5242880

class Player : public sf::SoundStream {
public:
	Player() ://int channelCount, int sampleRate) :
    m_currentSample		(0),
    // myChannelCount		(channelCount),
    // mySampleRate		(sampleRate),
    m_hasFinished		(false)

    {
    	//initialize(myChannelCount, mySampleRate);
    	//std::cout << myChannelCount << " " << mySampleRate << std::endl;
    }

	void load(const char * data, std::size_t sizeInBytes, int finished, int npart) {		//const sf::SoundBuffer& buffer) {
		// extract the audio samples from the sound buffer to our own container
		//m_samples.assign(buffer.getSamples(), buffer.getSamples() + buffer.getSampleCount() + m_samples.size());
		//m_samples.emplace_back(buffer.getSamples());
		//std::cout << data << std::endl;

		/*sf::MemoryInputStream tmp;
		tmp.open(data, sizeInBytes);*/

		if(npart == 0) {
			sf::SoundBuffer buffer;
		    buffer.loadFromMemory(data, sizeInBytes);
			initialize(buffer.getChannelCount(), buffer.getSampleRate());
			OnStart();
		}
		const sf::Int16* samples = reinterpret_cast<const sf::Int16*>(data);
		std::size_t sampleCount = (sizeInBytes / sizeof(sf::Int16));
		/*for(int i=0; i<sizeof(samples); i++)
			printf("%04x ", samples[i]);
		std::cout << std::endl;*/

		//std::cout << sampleCount << " " << sizeof(sf::Int16) << std::endl;

		{
			sf::Lock lock(m_mutex);
			std::copy(samples, samples + sampleCount, std::back_inserter(m_samples));
			//m_samples.assign(samples, samples + sampleCount);
			//std::copy(tmp, tmp.getSize(), std::back_inserter(m_samples));
			//m_samples.resize(*(samples), *(samples) + sampleCount);
		}

		m_hasFinished = (bool)finished;
		/*if(m_hasFinished) {
			std::cout << "Audio received" << std::endl;
		}*/
		//std::copy(samples, samples + sampleCount, std::back_inserter(m_samples));
		//m_samples.assign(samples, samples + sampleCount);
		//play();
		/*if(m_currentSample == 0) {
			// initialize the base class
			initialize(buffer.getChannelCount(), buffer.getSampleRate());
		}*/
	}

private:

	virtual void OnStart() {
		play();
	}

	virtual bool onGetData(sf::SoundStream::Chunk& data) {
		// We have reached the end of the buffer and all audio data have been played: we can stop playback
        if (m_currentSample >= m_samples.size() && m_hasFinished)
            return false;

        while ((m_currentSample >= m_samples.size()) && !m_hasFinished)
			sf::sleep(sf::milliseconds(100));

        // Copy samples into a local buffer to avoid synchronization problems
        // (don't forget that we run in two separate threads)
        {
            sf::Lock lock(m_mutex);
            m_tempBuffer.assign(m_samples.begin() + m_currentSample,  m_samples.end());
        }

        // Fill audio data to pass to the stream
        data.samples     = &m_tempBuffer[0];
        data.sampleCount = m_tempBuffer.size();

        // Update the playing offset
        m_currentSample += m_tempBuffer.size();

		return true;
	}

	/*virtual bool onGetData(Chunk& data) {
		// number of samples to stream every time the function is called;
        // in a more robust implementation, it should be a fixed
        // amount of time rather than an arbitrary number of samples
		const int samplesToStream = 44100;

		data.samples = &m_samples[m_currentSample];

		// have we reached the end of the sound?
        if (m_currentSample + samplesToStream <= m_samples.size())
        {
            // end not reached: stream the samples and continue
            data.sampleCount = samplesToStream;
            m_currentSample += samplesToStream;
            return true;
        }
        else
        {
            // end of stream reached: stream the remaining samples and stop playback
            data.sampleCount = m_samples.size() - m_currentSample;
            m_currentSample = m_samples.size();
            return false;
        }
	}*/

	virtual void onSeek(sf::Time timeOffset) {
		// compute the corresponding sample index according to the sample rate and channel count
        m_currentSample = (timeOffset.asMilliseconds() * getSampleRate() * getChannelCount()) / 1000;//static_cast<std::size_t>(timeOffset.asSeconds() * getSampleRate() * getChannelCount());
	}

	std::vector<sf::Int16> m_samples;
	std::size_t m_currentSample;
	std::vector<sf::Int16> m_tempBuffer;
	bool m_hasFinished;
	sf::Mutex m_mutex;
	int mySampleRate;
	int myChannelCount;
};

//#endif

int main(int argc, char** argv) {
	std::string filename = argv[1];
	FILE* f;
	size_t sz, offset = 0;
	//sf::SoundBuffer buffer;
    // buffer.loadFromFile(filename);
	//Player player(buffer.getChannelCount(), buffer.getSampleRate());
	Player player;
	int npart = 0;
	while(true) {
		f = fopen(filename.c_str(), "rb");
		//fseek(f, offset, SEEK_SET);
		char *data = (char*) malloc (sizeof(char)*CHUNK_SIZE);
		//std::cout << data << std::endl;
		sz = fread(data, 1, CHUNK_SIZE, f);
		fclose(f);
		//std::cout << sz << std::endl;
		//std::cout << data << std::endl;
		/*for(int i=0; i<sizeof(data); i++)
			printf("%04x ", data[i]);
		std::cout << std::endl;
		std::cout << sz << " " << sizeof(char) << std::endl;*/
		if(sz < CHUNK_SIZE) {
			//std::cout << data << std::endl;
			player.load(data, sz, 1, npart++);
			free(data);
			break;
		}
		player.load(data, sz, 0, npart++);
		free(data);
		//offset += sz;
		//mem_data.open((void *)data, sz);
		//buffer.loadFromMemory((void **)data, sz);
	}
	//player.play();
	while (player.getStatus() != Player::Stopped)
        sf::sleep(sf::milliseconds(100));

    return 0;
}