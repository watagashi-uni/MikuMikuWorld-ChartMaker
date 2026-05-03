#pragma once

#include "ScoreSerializer.h"

namespace MikuMikuWorld
{
	class CustomScoreJsonSerializer : public ScoreSerializer
	{
	public:
		void serialize(const Score& score, std::string filename) override;
		Score deserialize(std::string filename) override;
	};
}
