#include "CustomScoreJsonSerializer.h"

#include "Constants.h"
#include "File.h"
#include "IO.h"
#include "JsonIO.h"
#include "Note.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace MikuMikuWorld
{
	using json = nlohmann::json;

	namespace custom_score_json
	{
		struct RawNote
		{
			int id{};
			int ticks{};
			int laneStart{};
			int laneEnd{};
			int category{};
			int noteBaseType{};
			int previousConnectionId{ -1 };
			int nextConnectionId{ -1 };
			int direction{};
			int noteLineType{};
			bool critical{};
			bool isSkip{};
		};

		enum class SlideKind
		{
			Start,
			End,
			Relay,
			Invisible,
		};

		int toInt(const json& js, const char* key, int fallback = 0)
		{
			const auto it = js.find(key);
			if (it == js.end() || it->is_null())
				return fallback;

			if (it->is_number_integer())
				return it->get<int>();

			if (it->is_number())
				return static_cast<int>(std::round(it->get<double>()));

			if (it->is_string())
				return std::atoi(it->get_ref<const std::string&>().c_str());

			return fallback;
		}

		float toFloat(const json& js, const char* key, float fallback = 0.0f)
		{
			const auto it = js.find(key);
			if (it == js.end() || it->is_null())
				return fallback;

			if (it->is_number())
				return it->get<float>();

			if (it->is_string())
				return std::strtof(it->get_ref<const std::string&>().c_str(), nullptr);

			return fallback;
		}

		bool toBool(const json& js, const char* key, bool fallback = false)
		{
			const auto it = js.find(key);
			if (it == js.end() || it->is_null())
				return fallback;

			if (it->is_boolean())
				return it->get<bool>();

			if (it->is_number())
				return it->get<int>() != 0;

			return fallback;
		}

		bool looksLikeCustomScoreJson(const json& js)
		{
			if (!js.is_object())
				return false;

			return jsonIO::arrayHasData(js, "MusicScoreEventDataList")
				|| jsonIO::arrayHasData(js, "NoteList")
				|| (jsonIO::keyExists(js, "MusicScoreEventDataList") && js["MusicScoreEventDataList"].is_array())
				|| (jsonIO::keyExists(js, "NoteList") && js["NoteList"].is_array());
		}

		int lane(const RawNote& note)
		{
			return std::clamp(note.laneStart, MIN_LANE, MAX_LANE);
		}

		int width(const RawNote& note)
		{
			return std::clamp(note.laneEnd - note.laneStart + 1, MIN_NOTE_WIDTH, MAX_NOTE_WIDTH);
		}

		bool isVisibleRelaySlideNote(const RawNote& note)
		{
			return note.noteBaseType == 5 || note.category == 2;
		}

		bool shouldVisibleRelayAffectPath(const RawNote& note)
		{
			return isVisibleRelaySlideNote(note) && !note.isSkip;
		}

		bool isVisibleRelayAttachment(const RawNote& note)
		{
			return isVisibleRelaySlideNote(note) && !shouldVisibleRelayAffectPath(note);
		}

		bool isDecorationSlideNote(const RawNote& note)
		{
			return note.category == 9 || note.noteBaseType == 10 || note.noteBaseType == 13;
		}

		bool isDecorationSlideChain(const std::vector<const RawNote*>& chain)
		{
			return std::any_of(chain.begin(), chain.end(), [](const RawNote* note)
			{
				return note != nullptr && isDecorationSlideNote(*note);
			});
		}

		SlideKind getSlideKind(const RawNote& note, bool isLast)
		{
			if (isLast)
				return SlideKind::End;

			const int base = note.noteBaseType;
			if (base == 2 || base == 8 || base == 9 || base == 10)
				return SlideKind::Start;

			if (base == 1 || base == 3 || base == 11 || base == 12 || base == 13)
				return SlideKind::End;

			if (base == 6 || base == 14 || note.category == 11)
				return SlideKind::Invisible;

			return SlideKind::Relay;
		}

		bool isCancelNote(const RawNote& note)
		{
			return note.noteBaseType == 9 || note.noteBaseType == 12 || note.noteBaseType == 10 || note.noteBaseType == 13;
		}

		bool isTraceNote(const RawNote& note)
		{
			return note.noteBaseType == 4 || note.noteBaseType == 11 || note.category == 4 || note.category == 8;
		}

		bool isTraceFlickNote(const RawNote& note)
		{
			return note.noteBaseType == 4 || note.category == 8;
		}

		bool isFlickNote(const RawNote& note)
		{
			return note.noteBaseType == 3 || note.category == 3;
		}

		FlickType flickTypeFromDirection(int direction)
		{
			if (direction == 1)
				return FlickType::Left;

			if (direction == 2)
				return FlickType::Right;

			return FlickType::Default;
		}

		EaseType easeFromLineType(int noteLineType)
		{
			if (noteLineType == 2)
				return EaseType::EaseIn;

			if (noteLineType == 1)
				return EaseType::EaseOut;

			return EaseType::Linear;
		}

		RawNote readNote(const json& js)
		{
			return RawNote{
				toInt(js, "id"),
				toInt(js, "ticks"),
				toInt(js, "laneStart"),
				toInt(js, "laneEnd"),
				toInt(js, "category"),
				toInt(js, "noteBaseType"),
				toInt(js, "previousConnectionId", -1),
				toInt(js, "nextConnectionId", -1),
				toInt(js, "direction"),
				toInt(js, "noteLineType"),
				toBool(js, "type"),
				toBool(js, "isSkip"),
			};
		}

		void addTap(Score& score, const RawNote& raw, bool forceCritical = false)
		{
			if (isCancelNote(raw))
				return;

			Note note(NoteType::Tap, raw.ticks, lane(raw), width(raw));
			note.ID = nextID++;
			note.critical = forceCritical || raw.critical;
			note.friction = isTraceNote(raw);
			if (isFlickNote(raw) || isTraceFlickNote(raw) || raw.direction == 1 || raw.direction == 2)
				note.flick = flickTypeFromDirection(raw.direction);

			score.notes[note.ID] = note;
		}

		HoldStepType stepTypeFromRaw(const RawNote& raw, SlideKind kind)
		{
			if (kind == SlideKind::Invisible)
				return HoldStepType::Hidden;

			if (isVisibleRelayAttachment(raw))
				return HoldStepType::Skip;

			return HoldStepType::Normal;
		}

		HoldNoteType endpointTypeFromRaw(const RawNote& raw, bool decoration)
		{
			if (decoration)
				return HoldNoteType::Guide;

			return isCancelNote(raw) ? HoldNoteType::Hidden : HoldNoteType::Normal;
		}

		std::vector<const RawNote*> removeAdjacentVisibleRelayDuplicates(const std::vector<const RawNote*>& chain)
		{
			std::vector<const RawNote*> filtered;
			filtered.reserve(chain.size());
			for (size_t index = 0; index < chain.size(); ++index)
			{
				const RawNote* note = chain[index];
				const RawNote* next = index + 1 < chain.size() ? chain[index + 1] : nullptr;
				if (note != nullptr && next != nullptr
					&& isVisibleRelaySlideNote(*note)
					&& isVisibleRelaySlideNote(*next)
					&& std::abs(next->ticks - note->ticks) <= 1)
				{
					continue;
				}

				filtered.push_back(note);
			}

			return filtered;
		}

		std::vector<std::vector<const RawNote*>> buildChains(
			const std::vector<RawNote>& notes,
			const std::unordered_map<int, const RawNote*>& byId,
			std::unordered_set<int>& connectedIds)
		{
			std::vector<std::vector<const RawNote*>> chains;

			for (const auto& note : notes)
			{
				if (connectedIds.find(note.id) != connectedIds.end())
					continue;

				if (note.nextConnectionId == -1 && note.previousConnectionId == -1)
					continue;

				if (note.previousConnectionId != -1)
					continue;

				std::vector<const RawNote*> chain;
				const RawNote* current = &note;
				while (current != nullptr && connectedIds.find(current->id) == connectedIds.end())
				{
					chain.push_back(current);
					connectedIds.insert(current->id);
					if (current->nextConnectionId == -1)
					{
						current = nullptr;
					}
					else
					{
						const auto it = byId.find(current->nextConnectionId);
						current = it == byId.end() ? nullptr : it->second;
					}
				}

				if (!chain.empty())
					chains.push_back(chain);
			}

			for (const auto& note : notes)
			{
				if (connectedIds.find(note.id) == connectedIds.end()
					&& (note.nextConnectionId != -1 || note.previousConnectionId != -1))
				{
					chains.push_back({ &note });
					connectedIds.insert(note.id);
				}
			}

			return chains;
		}

		void addChain(Score& score, const std::vector<const RawNote*>& rawChain)
		{
			const std::vector<const RawNote*> chain = removeAdjacentVisibleRelayDuplicates(rawChain);
			if (chain.size() < 2 || chain.front() == nullptr || chain.back() == nullptr)
				return;

			const bool decoration = isDecorationSlideChain(chain);
			const int startID = nextID++;
			HoldNote hold{};

			for (size_t index = 0; index < chain.size(); ++index)
			{
				const RawNote& raw = *chain[index];
				const bool isFirst = index == 0;
				const bool isLast = index + 1 == chain.size();
				const SlideKind kind = getSlideKind(raw, isLast);
				const NoteType noteType = isFirst ? NoteType::Hold : (isLast ? NoteType::HoldEnd : NoteType::HoldMid);

				Note note(noteType, raw.ticks, lane(raw), width(raw));
				note.ID = isFirst ? startID : nextID++;
				note.parentID = isFirst ? -1 : startID;
				note.critical = raw.critical;
				note.friction = isTraceNote(raw);
				if (isFlickNote(raw) || isTraceFlickNote(raw) || raw.direction == 1 || raw.direction == 2)
					note.flick = flickTypeFromDirection(raw.direction);

				if (isFirst)
				{
					hold.start = HoldStep{ note.ID, HoldStepType::Normal, easeFromLineType(raw.noteLineType) };
					hold.startType = endpointTypeFromRaw(raw, decoration);
				}
				else if (isLast)
				{
					hold.end = note.ID;
					hold.endType = endpointTypeFromRaw(raw, decoration);
				}
				else
				{
					hold.steps.push_back(HoldStep{ note.ID, stepTypeFromRaw(raw, kind), easeFromLineType(raw.noteLineType) });
				}

				score.notes[note.ID] = note;

				if (!decoration && endpointTypeFromRaw(raw, false) == HoldNoteType::Hidden && raw.critical)
					addTap(score, raw, true);
			}

			if (hold.start.ID == 0 || hold.end == 0)
				return;

			sortHoldSteps(score, hold);
			score.holdNotes[startID] = hold;
		}

		Score parse(const std::string& text, float normalizedOffsetMs)
		{
			const json root = json::parse(text);
			if (!looksLikeCustomScoreJson(root))
				throw std::runtime_error("Not a supported ChartMaker JSON file.");

			Score score{};
			score.metadata.musicOffset = normalizedOffsetMs;
			score.tempoChanges.clear();
			score.hiSpeedChanges.clear();

			const auto eventList = root.find("MusicScoreEventDataList");
			if (eventList != root.end() && eventList->is_array())
			{
				for (const auto& event : *eventList)
				{
					const int type = toInt(event, "eventType", -1);
					const int tick = toInt(event, "ticks");
					if (type == 0)
						score.tempoChanges.push_back({ tick, toFloat(event, "changeValue", 120.0f) });
					else if (type == 2)
						score.hiSpeedChanges.push_back({ tick, toFloat(event, "changeValue", 1.0f) });
				}
			}

			if (score.tempoChanges.empty())
				score.tempoChanges.push_back({ 0, 120.0f });

			std::stable_sort(score.tempoChanges.begin(), score.tempoChanges.end(),
				[](const Tempo& left, const Tempo& right) { return left.tick < right.tick; });
			std::stable_sort(score.hiSpeedChanges.begin(), score.hiSpeedChanges.end(),
				[](const HiSpeedChange& left, const HiSpeedChange& right) { return left.tick < right.tick; });

			std::vector<RawNote> notes;
			const auto noteList = root.find("NoteList");
			if (noteList != root.end() && noteList->is_array())
			{
				notes.reserve(noteList->size());
				for (const auto& noteJson : *noteList)
					notes.push_back(readNote(noteJson));
			}

			std::stable_sort(notes.begin(), notes.end(), [](const RawNote& left, const RawNote& right)
			{
				if (left.ticks != right.ticks)
					return left.ticks < right.ticks;

				if (left.laneStart != right.laneStart)
					return left.laneStart < right.laneStart;

				return left.id < right.id;
			});

			std::unordered_map<int, const RawNote*> byId;
			byId.reserve(notes.size());
			for (const auto& note : notes)
				byId[note.id] = &note;

			std::unordered_set<int> connectedIds;
			connectedIds.reserve(notes.size());
			const auto chains = buildChains(notes, byId, connectedIds);
			for (const auto& chain : chains)
				addChain(score, chain);

			for (const auto& note : notes)
			{
				if (connectedIds.find(note.id) == connectedIds.end())
					addTap(score, note);
			}

			return score;
		}
	}

	void CustomScoreJsonSerializer::serialize(const Score& score, std::string filename)
	{
		throw std::runtime_error("Exporting ChartMaker JSON is not supported.");
	}

	Score CustomScoreJsonSerializer::deserialize(std::string filename)
	{
		if (!IO::File::exists(filename))
			throw std::runtime_error("Score JSON file not found.");

		IO::File scoreFile(filename, IO::FileMode::ReadBinary);
		std::vector<uint8_t> bytes = scoreFile.readAllBytes();
		scoreFile.close();

		if (IO::isGzipCompressed(bytes))
			bytes = IO::inflateGzip(bytes);

		return custom_score_json::parse(std::string(bytes.begin(), bytes.end()), 0.0f);
	}
}
