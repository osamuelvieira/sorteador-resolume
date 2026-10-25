#pragma once
#include <FFGLSDK.h>
#include "ChannelManager.h"
#include "TextRenderer.h"
#include <array>
#include <cstdint>
#include <vector>

namespace sorteador
{
// The FFGL source Resolume loads. Each instance renders one channel; its parameters read and write that
// channel's settings, so the Resolume panel and the web panel always show the same values.
class SorteadorPlugin : public CFFGLPlugin
{
public:
	SorteadorPlugin();

	FFResult InitGL( const FFGLViewportStruct* vp ) override;
	FFResult DeInitGL() override;
	FFResult ProcessOpenGL( ProcessOpenGLStruct* pGL ) override;

	FFResult SetFloatParameter( unsigned int index, float value ) override;
	float GetFloatParameter( unsigned int index ) override;
	FFResult SetTextParameter( unsigned int index, const char* value ) override;
	char* GetTextParameter( unsigned int index ) override;
	char* GetParameterDisplay( unsigned int index ) override;

private:
	void AddRangeParam( unsigned int index, const char* name, float value, float min, float max, const char* group );
	void AddColorParams( unsigned int redIndex, const char* name, const Color& color, const char* group );
	void RaiseValueEvents();

	TextRenderer text;
	int channel                 = 0;
	uint64_t seenConfigRevision = 0;
	std::array< float, 64 > previousEventValue{};
	std::vector< char > textBuffer;
	std::vector< char > displayBuffer;
};

}//namespace sorteador
