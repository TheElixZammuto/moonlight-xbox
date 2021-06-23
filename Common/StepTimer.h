#pragma once

#include <wrl.h>

namespace DX
{
	// Classi di supporto per l'intervallo di animazione e simulazione.
	class StepTimer
	{
	public:
		StepTimer() : 
			m_elapsedTicks(0),
			m_totalTicks(0),
			m_leftOverTicks(0),
			m_frameCount(0),
			m_framesPerSecond(0),
			m_framesThisSecond(0),
			m_qpcSecondCounter(0),
			m_isFixedTimeStep(false),
			m_targetElapsedTicks(TicksPerSecond / 60)
		{
			if (!QueryPerformanceFrequency(&m_qpcFrequency))
			{
				throw ref new Platform::FailureException();
			}

			if (!QueryPerformanceCounter(&m_qpcLastTime))
			{
				throw ref new Platform::FailureException();
			}

			// Inizializza un delta massimo su 1/10 di secondo.
			m_qpcMaxDelta = m_qpcFrequency.QuadPart / 10;
		}

		// Ottiene il tempo trascorso dalla precedente chiamata di aggiornamento.
		uint64 GetElapsedTicks() const						{ return m_elapsedTicks; }
		double GetElapsedSeconds() const					{ return TicksToSeconds(m_elapsedTicks); }

		// Ottiene il tempo totale dall'avvio del programma.
		uint64 GetTotalTicks() const						{ return m_totalTicks; }
		double GetTotalSeconds() const						{ return TicksToSeconds(m_totalTicks); }

		// Ottiene il numero totale di aggiornamenti dall'avvio del programma.
		uint32 GetFrameCount() const						{ return m_frameCount; }

		// Ottiene la frequenza di fotogrammi corrente.
		uint32 GetFramesPerSecond() const					{ return m_framesPerSecond; }

		// Consente di scegliere se usare la modalità timestep fisso o variabile.
		void SetFixedTimeStep(bool isFixedTimestep)			{ m_isFixedTimeStep = isFixedTimestep; }

		// Consente di impostare la frequenza di chiamata dell'aggiornamento in modalità timestep fisso.
		void SetTargetElapsedTicks(uint64 targetElapsed)	{ m_targetElapsedTicks = targetElapsed; }
		void SetTargetElapsedSeconds(double targetElapsed)	{ m_targetElapsedTicks = SecondsToTicks(targetElapsed); }

		// Il formato Integer rappresenta l'ora usando 10.000.000 cicli al secondo.
		static const uint64 TicksPerSecond = 10000000;

		static double TicksToSeconds(uint64 ticks)			{ return static_cast<double>(ticks) / TicksPerSecond; }
		static uint64 SecondsToTicks(double seconds)		{ return static_cast<uint64>(seconds * TicksPerSecond); }

		// Dopo una discontinuità di intervallo intenzionale (ad esempio un'operazione I/O di blocco)
		// effettuare la chiamata per evitare che la logica timestep fisso provi a effettuare un set di 
		// chiamate Update di recupero.

		void ResetElapsedTime()
		{
			if (!QueryPerformanceCounter(&m_qpcLastTime))
			{
				throw ref new Platform::FailureException();
			}

			m_leftOverTicks = 0;
			m_framesPerSecond = 0;
			m_framesThisSecond = 0;
			m_qpcSecondCounter = 0;
		}

		// Aggiorna lo stato del timer, chiamando la funzione Update specificata il numero appropriato di volte.
		template<typename TUpdate>
		void Tick(const TUpdate& update)
		{
			// Esegue una query per conoscere l'ora corrente.
			LARGE_INTEGER currentTime;

			if (!QueryPerformanceCounter(&currentTime))
			{
				throw ref new Platform::FailureException();
			}

			uint64 timeDelta = currentTime.QuadPart - m_qpcLastTime.QuadPart;

			m_qpcLastTime = currentTime;
			m_qpcSecondCounter += timeDelta;

			// Applica un clamp a delta di tempo eccessivamente ampi, ad esempio, dopo la pausa nel debugger.
			if (timeDelta > m_qpcMaxDelta)
			{
				timeDelta = m_qpcMaxDelta;
			}

			// Converte unità QPC in un formato tick canonico. Non è possibile eseguirne l'overflow a causa del clamp precedente.
			timeDelta *= TicksPerSecond;
			timeDelta /= m_qpcFrequency.QuadPart;

			uint32 lastFrameCount = m_frameCount;

			if (m_isFixedTimeStep)
			{
				// Logica di aggiornamento del timestep fisso

				// Se l'app è in esecuzione quando sta per scadere il tempo trascorso di destinazione (entro 1/4 di millisecondo), è sufficiente applicare un clamp
				// all'orologio in modo che corrisponda esattamente al valore di destinazione per evitare l'accumulo nel tempo di
				// piccoli errori irrilevanti. Senza questo clamp, un gioco che ha richiesto un aggiornamento
				// fisso pari a 60 fps, in esecuzione con vsync abilitato su un monitor NTSC 59.94, accumulerà
				// alla fine un numero di piccoli errori tale da causare l'eliminazione di un fotogramma. È preferibile arrotondare 
				// piccole deviazioni fino a zero per consentire un'esecuzione senza problemi.

				if (abs(static_cast<int64>(timeDelta - m_targetElapsedTicks)) < TicksPerSecond / 4000)
				{
					timeDelta = m_targetElapsedTicks;
				}

				m_leftOverTicks += timeDelta;

				while (m_leftOverTicks >= m_targetElapsedTicks)
				{
					m_elapsedTicks = m_targetElapsedTicks;
					m_totalTicks += m_targetElapsedTicks;
					m_leftOverTicks -= m_targetElapsedTicks;
					m_frameCount++;

					update();
				}
			}
			else
			{
				// Logica di aggiornamento del timestep variabile.
				m_elapsedTicks = timeDelta;
				m_totalTicks += timeDelta;
				m_leftOverTicks = 0;
				m_frameCount++;

				update();
			}

			// Traccia la frequenza dei fotogrammi corrente.
			if (m_frameCount != lastFrameCount)
			{
				m_framesThisSecond++;
			}

			if (m_qpcSecondCounter >= static_cast<uint64>(m_qpcFrequency.QuadPart))
			{
				m_framesPerSecond = m_framesThisSecond;
				m_framesThisSecond = 0;
				m_qpcSecondCounter %= m_qpcFrequency.QuadPart;
			}
		}

	private:
		// Nei dati dell'intervallo di origine vengono usate unità QPC.
		LARGE_INTEGER m_qpcFrequency;
		LARGE_INTEGER m_qpcLastTime;
		uint64 m_qpcMaxDelta;

		// I dati di intervallo derivati utilizzano un formato tick canonico.
		uint64 m_elapsedTicks;
		uint64 m_totalTicks;
		uint64 m_leftOverTicks;

		// Membri usati per tracciare la frequenza dei fotogrammi.
		uint32 m_frameCount;
		uint32 m_framesPerSecond;
		uint32 m_framesThisSecond;
		uint64 m_qpcSecondCounter;

		// Membri usati per configurare la modalità timestep fisso.
		bool m_isFixedTimeStep;
		uint64 m_targetElapsedTicks;
	};
}
