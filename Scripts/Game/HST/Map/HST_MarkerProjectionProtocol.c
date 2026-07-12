class HST_MarkerProjectionPacket
{
	string m_sKind;
	int m_iProtocolVersion;
	int m_iEpoch;
	string m_sSnapshotId;
	int m_iWatermark;
	int m_iChunkIndex;
	int m_iChunkCount;
	int m_iTotalRecordCount;
	int m_iFromSequence;
	int m_iToSequence;
	string m_sRegistryHash;
	ref array<ref HST_MapMarkerState> m_aRecords = {};
}

class HST_MarkerProjectionApplyResult
{
	bool m_bAccepted;
	bool m_bSnapshotCommitted;
	bool m_bRegistryChanged;
	bool m_bNeedsAcknowledge;
	bool m_bNeedsResync;
	int m_iEpoch;
	int m_iAcknowledgeSequence;
	string m_sSnapshotId;
	string m_sRegistryHash;
	string m_sReason;
}

class HST_MarkerProjectionDispatch
{
	ref array<string> m_aPackets = {};
	bool m_bSnapshot;
	int m_iEpoch;
	int m_iFromSequence;
	int m_iToSequence;
	string m_sSnapshotId;
	string m_sRegistryHash;
	string m_sReason;

	bool HasPackets()
	{
		return !m_aPackets.IsEmpty();
	}
}

class HST_MarkerProjectionCodec
{
	static const int PROTOCOL_VERSION = 1;
	static const int MAX_PACKET_CHARACTERS = 12000;
	static const int MAX_RECORDS_PER_PACKET = 32;
	static const int MAX_SNAPSHOT_CHUNKS = 64;
	static const int MAX_SNAPSHOT_RECORDS = 2048;
	static const int MAX_DELTA_DISPATCH_RECORDS = 512;
	static const string SNAPSHOT_HEADER = "HST_MARKER_SNAPSHOT";
	static const string DELTA_HEADER = "HST_MARKER_DELTA";
	static const string RECORD_HEADER = "M";

	static HST_MapMarkerState CopyMarker(HST_MapMarkerState source)
	{
		if (!source)
			return null;

		HST_MapMarkerState target = new HST_MapMarkerState();
		target.m_sMarkerId = source.m_sMarkerId;
		target.m_sLinkedId = source.m_sLinkedId;
		target.m_sLabel = source.m_sLabel;
		target.m_sCallsign = source.m_sCallsign;
		target.m_sCategory = source.m_sCategory;
		target.m_sOwnerFactionKey = source.m_sOwnerFactionKey;
		target.m_sIconHint = source.m_sIconHint;
		target.m_sColorHint = source.m_sColorHint;
		target.m_sTextColorHint = source.m_sTextColorHint;
		target.m_sStyleHint = source.m_sStyleHint;
		target.m_vPosition = source.m_vPosition;
		target.m_bVisible = source.m_bVisible;
		target.m_bRuntimeNative = source.m_bRuntimeNative;
		target.m_iRevision = source.m_iRevision;
		target.m_iStreamSequence = source.m_iStreamSequence;
		target.m_bTombstone = source.m_bTombstone;
		target.m_iTombstonedAtSecond = source.m_iTombstonedAtSecond;
		return target;
	}

	static bool ContentEquals(HST_MapMarkerState left, HST_MapMarkerState right)
	{
		if (!left || !right)
			return left == right;

		if (left.m_sMarkerId != right.m_sMarkerId || left.m_sLinkedId != right.m_sLinkedId)
			return false;
		if (left.m_sLabel != right.m_sLabel || left.m_sCallsign != right.m_sCallsign)
			return false;
		if (left.m_sCategory != right.m_sCategory || left.m_sOwnerFactionKey != right.m_sOwnerFactionKey)
			return false;
		if (left.m_sIconHint != right.m_sIconHint || left.m_sColorHint != right.m_sColorHint)
			return false;
		if (left.m_sTextColorHint != right.m_sTextColorHint || left.m_sStyleHint != right.m_sStyleHint)
			return false;
		if (left.m_vPosition != right.m_vPosition)
			return false;
		if (left.m_bVisible != right.m_bVisible || left.m_bRuntimeNative != right.m_bRuntimeNative)
			return false;
		return left.m_bTombstone == right.m_bTombstone;
	}

	static bool TransportEquals(HST_MapMarkerState left, HST_MapMarkerState right)
	{
		return ContentEquals(left, right)
			&& left.m_iRevision == right.m_iRevision
			&& left.m_iStreamSequence == right.m_iStreamSequence
			&& left.m_iTombstonedAtSecond == right.m_iTombstonedAtSecond;
	}

	static string BuildContentSignature(HST_MapMarkerState marker)
	{
		if (!marker)
			return "";

		string signature = EncodeField(marker.m_sMarkerId) + "|" + EncodeField(marker.m_sLinkedId);
		signature = signature + "|" + EncodeField(marker.m_sLabel) + "|" + EncodeField(marker.m_sCallsign);
		signature = signature + "|" + EncodeField(marker.m_sCategory) + "|" + EncodeField(marker.m_sOwnerFactionKey);
		signature = signature + "|" + EncodeField(marker.m_sIconHint) + "|" + EncodeField(marker.m_sColorHint);
		signature = signature + "|" + EncodeField(marker.m_sTextColorHint) + "|" + EncodeField(marker.m_sStyleHint);
		signature = signature + string.Format("|%1|%2|%3", marker.m_vPosition[0], marker.m_vPosition[1], marker.m_vPosition[2]);
		signature = signature + string.Format("|%1|%2|%3|%4", marker.m_bVisible, marker.m_bRuntimeNative, marker.m_iRevision, marker.m_bTombstone);
		return signature;
	}

	static string BuildLiveRegistryHash(map<string, ref HST_MapMarkerState> records)
	{
		if (!records)
			return "0";

		array<string> ids = {};
		foreach (string id, HST_MapMarkerState marker : records)
		{
			if (marker && !marker.m_bTombstone)
				ids.Insert(id);
		}
		ids.Sort();

		string canonical;
		foreach (string markerId : ids)
		{
			HST_MapMarkerState marker = records.Get(markerId);
			canonical = canonical + "\n" + BuildContentSignature(marker);
		}

		return string.Format("%1:%2", ids.Count(), canonical.Hash());
	}

	static string EncodeRecord(HST_MapMarkerState marker)
	{
		if (!marker)
			return "";

		string line = string.Format("M|%1|%2|%3|%4", marker.m_iStreamSequence, marker.m_iRevision, BoolValue(marker.m_bTombstone), marker.m_iTombstonedAtSecond);
		line = line + "|" + EncodeField(marker.m_sMarkerId) + "|" + EncodeField(marker.m_sLinkedId);
		line = line + "|" + EncodeField(marker.m_sLabel) + "|" + EncodeField(marker.m_sCallsign);
		line = line + "|" + EncodeField(marker.m_sCategory) + "|" + EncodeField(marker.m_sOwnerFactionKey);
		line = line + "|" + EncodeField(marker.m_sIconHint) + "|" + EncodeField(marker.m_sColorHint);
		line = line + "|" + EncodeField(marker.m_sTextColorHint) + "|" + EncodeField(marker.m_sStyleHint);
		line = line + string.Format("|%1|%2|%3|%4|%5", marker.m_vPosition[0], marker.m_vPosition[1], marker.m_vPosition[2], BoolValue(marker.m_bVisible), BoolValue(marker.m_bRuntimeNative));
		return line;
	}

	static HST_MapMarkerState DecodeRecord(string line)
	{
		array<string> fields = {};
		line.Split("|", fields, false);
		if (fields.Count() != 20 || fields[0] != RECORD_HEADER)
			return null;
		if (!IsStrictInteger(fields[1], false)
			|| !IsStrictInteger(fields[2], false)
			|| !IsStrictBoolean(fields[3])
			|| !IsStrictInteger(fields[4], false)
			|| !IsStrictFloat(fields[15])
			|| !IsStrictFloat(fields[16])
			|| !IsStrictFloat(fields[17])
			|| !IsStrictBoolean(fields[18])
			|| !IsStrictBoolean(fields[19]))
			return null;

		HST_MapMarkerState marker = new HST_MapMarkerState();
		marker.m_iStreamSequence = fields[1].ToInt();
		marker.m_iRevision = fields[2].ToInt();
		marker.m_bTombstone = ParseBool(fields[3]);
		marker.m_iTombstonedAtSecond = fields[4].ToInt();
		marker.m_sMarkerId = DecodeField(fields[5]);
		marker.m_sLinkedId = DecodeField(fields[6]);
		marker.m_sLabel = DecodeField(fields[7]);
		marker.m_sCallsign = DecodeField(fields[8]);
		marker.m_sCategory = DecodeField(fields[9]);
		marker.m_sOwnerFactionKey = DecodeField(fields[10]);
		marker.m_sIconHint = DecodeField(fields[11]);
		marker.m_sColorHint = DecodeField(fields[12]);
		marker.m_sTextColorHint = DecodeField(fields[13]);
		marker.m_sStyleHint = DecodeField(fields[14]);
		marker.m_vPosition[0] = fields[15].ToFloat();
		marker.m_vPosition[1] = fields[16].ToFloat();
		marker.m_vPosition[2] = fields[17].ToFloat();
		marker.m_bVisible = ParseBool(fields[18]);
		marker.m_bRuntimeNative = ParseBool(fields[19]);
		if (marker.m_sMarkerId.IsEmpty() || marker.m_iRevision <= 0 || marker.m_iStreamSequence <= 0)
			return null;
		if (marker.m_bTombstone && marker.m_bVisible)
			return null;

		return marker;
	}

	static HST_MarkerProjectionPacket DecodePacket(string payload)
	{
		if (payload.IsEmpty() || payload.Length() > MAX_PACKET_CHARACTERS)
			return null;

		array<string> lines = {};
		payload.Split("\n", lines, false);
		if (lines.IsEmpty())
			return null;

		array<string> header = {};
		lines[0].Split("|", header, false);
		if (header.IsEmpty())
			return null;

		HST_MarkerProjectionPacket packet = new HST_MarkerProjectionPacket();
		packet.m_sKind = header[0];
		if (packet.m_sKind == SNAPSHOT_HEADER)
		{
			if (header.Count() != 9)
				return null;
			if (!IsStrictInteger(header[1], false)
				|| !IsStrictInteger(header[2], false)
				|| !IsStrictInteger(header[4], false)
				|| !IsStrictInteger(header[5], false)
				|| !IsStrictInteger(header[6], false)
				|| !IsStrictInteger(header[7], false))
				return null;

			packet.m_iProtocolVersion = header[1].ToInt();
			packet.m_iEpoch = header[2].ToInt();
			packet.m_sSnapshotId = DecodeField(header[3]);
			packet.m_iWatermark = header[4].ToInt();
			packet.m_iChunkIndex = header[5].ToInt();
			packet.m_iChunkCount = header[6].ToInt();
			packet.m_iTotalRecordCount = header[7].ToInt();
			packet.m_sRegistryHash = DecodeField(header[8]);
			if (packet.m_sSnapshotId.IsEmpty() || packet.m_iEpoch <= 0 || packet.m_iWatermark < 0 || packet.m_iChunkIndex < 0 || packet.m_iChunkCount <= 0 || packet.m_iChunkCount > MAX_SNAPSHOT_CHUNKS || packet.m_iChunkIndex >= packet.m_iChunkCount || packet.m_iTotalRecordCount < 0 || packet.m_iTotalRecordCount > MAX_SNAPSHOT_RECORDS)
				return null;
		}
		else if (packet.m_sKind == DELTA_HEADER)
		{
			if (header.Count() != 7)
				return null;
			if (!IsStrictInteger(header[1], false)
				|| !IsStrictInteger(header[2], false)
				|| !IsStrictInteger(header[3], false)
				|| !IsStrictInteger(header[4], false)
				|| !IsStrictInteger(header[5], false))
				return null;

			packet.m_iProtocolVersion = header[1].ToInt();
			packet.m_iEpoch = header[2].ToInt();
			packet.m_iFromSequence = header[3].ToInt();
			packet.m_iToSequence = header[4].ToInt();
			packet.m_iTotalRecordCount = header[5].ToInt();
			packet.m_sRegistryHash = DecodeField(header[6]);
			if (packet.m_iEpoch <= 0 || packet.m_iFromSequence <= 0 || packet.m_iToSequence < packet.m_iFromSequence || packet.m_iTotalRecordCount <= 0 || packet.m_iTotalRecordCount > MAX_RECORDS_PER_PACKET)
				return null;
		}
		else
		{
			return null;
		}

		if (packet.m_iProtocolVersion != PROTOCOL_VERSION)
			return null;

		for (int i = 1; i < lines.Count(); i++)
		{
			if (lines[i].IsEmpty())
				continue;

			HST_MapMarkerState marker = DecodeRecord(lines[i]);
			if (!marker)
				return null;
			packet.m_aRecords.Insert(marker);
		}

		if (packet.m_sKind == DELTA_HEADER && packet.m_aRecords.Count() != packet.m_iTotalRecordCount)
			return null;

		return packet;
	}

	static bool BuildSnapshotPackets(
		map<string, ref HST_MapMarkerState> records,
		int epoch,
		string snapshotId,
		int watermark,
		string registryHash,
		notnull array<string> packets)
	{
		packets.Clear();
		if (epoch <= 0 || snapshotId.IsEmpty() || watermark < 0)
			return false;
		array<string> ids = {};
		if (records)
		{
			foreach (string id, HST_MapMarkerState marker : records)
			{
				if (marker)
					ids.Insert(id);
			}
		}
		ids.Sort();
		if (ids.Count() > MAX_SNAPSHOT_RECORDS)
			return false;

		array<string> bodies = {};
		string body;
		int bodyRecords;
		foreach (string markerId : ids)
		{
			HST_MapMarkerState marker = records.Get(markerId);
			if (!marker || marker.m_iRevision <= 0 || marker.m_iStreamSequence <= 0 || (marker.m_bTombstone && marker.m_bVisible))
				return false;
			string line = EncodeRecord(marker);
			if (line.IsEmpty())
				return false;
			if (line.Length() > MAX_PACKET_CHARACTERS - 512)
				return false;
			if (bodyRecords >= MAX_RECORDS_PER_PACKET || (!body.IsEmpty() && body.Length() + line.Length() + 1 > MAX_PACKET_CHARACTERS - 256))
			{
				bodies.Insert(body);
				body = "";
				bodyRecords = 0;
			}

			if (!body.IsEmpty())
				body = body + "\n";
			body = body + line;
			bodyRecords++;
		}
		if (!body.IsEmpty() || bodies.IsEmpty())
			bodies.Insert(body);
		if (bodies.Count() > MAX_SNAPSHOT_CHUNKS)
		{
			packets.Clear();
			return false;
		}

		for (int chunkIndex; chunkIndex < bodies.Count(); chunkIndex++)
		{
			string header = string.Format(
				"%1|%2|%3|%4|%5|%6|%7|%8|%9",
				SNAPSHOT_HEADER,
				PROTOCOL_VERSION,
				epoch,
				EncodeField(snapshotId),
				watermark,
				chunkIndex,
				bodies.Count(),
				ids.Count(),
				EncodeField(registryHash));
			string packet = header;
			if (!bodies[chunkIndex].IsEmpty())
				packet = packet + "\n" + bodies[chunkIndex];
			if (packet.Length() > MAX_PACKET_CHARACTERS)
			{
				packets.Clear();
				return false;
			}
			packets.Insert(packet);
		}
		return !packets.IsEmpty();
	}

	static bool BuildDeltaPackets(
		notnull array<ref HST_MapMarkerState> orderedEvents,
		int epoch,
		string registryHash,
		notnull array<string> packets)
	{
		packets.Clear();
		if (epoch <= 0 || orderedEvents.IsEmpty() || orderedEvents.Count() > MAX_DELTA_DISPATCH_RECORDS)
			return false;
		array<string> bodies = {};
		array<int> fromSequences = {};
		array<int> toSequences = {};
		array<int> recordCounts = {};
		string body;
		int bodyRecords;
		int fromSequence;
		int toSequence;
		int expectedSequence;
		foreach (HST_MapMarkerState marker : orderedEvents)
		{
			if (!marker)
				return false;
			if (marker.m_iRevision <= 0 || marker.m_iStreamSequence <= 0 || (marker.m_bTombstone && marker.m_bVisible))
				return false;
			if (expectedSequence > 0 && marker.m_iStreamSequence != expectedSequence)
				return false;
			expectedSequence = marker.m_iStreamSequence + 1;
			string line = EncodeRecord(marker);
			if (line.IsEmpty())
				return false;
			if (line.Length() > MAX_PACKET_CHARACTERS - 256)
				return false;
			if (bodyRecords >= MAX_RECORDS_PER_PACKET || (!body.IsEmpty() && body.Length() + line.Length() + 1 > MAX_PACKET_CHARACTERS - 256))
			{
				bodies.Insert(body);
				fromSequences.Insert(fromSequence);
				toSequences.Insert(toSequence);
				recordCounts.Insert(bodyRecords);
				body = "";
				bodyRecords = 0;
				fromSequence = 0;
				toSequence = 0;
			}

			if (fromSequence <= 0)
				fromSequence = marker.m_iStreamSequence;
			toSequence = marker.m_iStreamSequence;
			if (!body.IsEmpty())
				body = body + "\n";
			body = body + line;
			bodyRecords++;
		}

		if (bodyRecords > 0)
		{
			bodies.Insert(body);
			fromSequences.Insert(fromSequence);
			toSequences.Insert(toSequence);
			recordCounts.Insert(bodyRecords);
		}

		for (int i; i < bodies.Count(); i++)
		{
			string packetHash;
			if (i == bodies.Count() - 1)
				packetHash = registryHash;
			string packet = BuildDeltaPacket(epoch, fromSequences[i], toSequences[i], recordCounts[i], packetHash, bodies[i]);
			if (packet.Length() > MAX_PACKET_CHARACTERS)
			{
				packets.Clear();
				return false;
			}
			packets.Insert(packet);
		}
		return !packets.IsEmpty();
	}

	protected static string BuildDeltaPacket(int epoch, int fromSequence, int toSequence, int recordCount, string registryHash, string body)
	{
		string header = string.Format(
			"%1|%2|%3|%4|%5|%6|%7",
			DELTA_HEADER,
			PROTOCOL_VERSION,
			epoch,
			fromSequence,
			toSequence,
			recordCount,
			EncodeField(registryHash));
		return header + "\n" + body;
	}

	static string EncodeField(string value)
	{
		value.Replace("%", "%25");
		value.Replace("\r", " ");
		value.Replace("\n", " ");
		value.Replace("|", "%7C");
		return value;
	}

	static string DecodeField(string value)
	{
		value.Replace("%7C", "|");
		value.Replace("%25", "%");
		return value;
	}

	static bool ParseBool(string value)
	{
		return value == "1" || value == "true";
	}

	protected static bool IsStrictBoolean(string value)
	{
		return value == "0" || value == "1";
	}

	protected static bool IsStrictInteger(string value, bool allowNegative = true)
	{
		if (value.IsEmpty())
			return false;
		int startIndex;
		if (value.StartsWith("-"))
		{
			if (!allowNegative || value.Length() == 1)
				return false;
			startIndex = 1;
		}
		for (int i = startIndex; i < value.Length(); i++)
		{
			if (!value.IsDigitAt(i))
				return false;
		}
		return true;
	}

	protected static bool IsStrictFloat(string value)
	{
		if (value.IsEmpty())
			return false;
		int startIndex;
		if (value.StartsWith("-"))
		{
			if (value.Length() == 1)
				return false;
			startIndex = 1;
		}
		bool foundDigit;
		bool foundDecimal;
		for (int i = startIndex; i < value.Length(); i++)
		{
			if (value.IsDigitAt(i))
			{
				foundDigit = true;
				continue;
			}
			if (!foundDecimal && value.Substring(i, 1) == ".")
			{
				foundDecimal = true;
				continue;
			}
			return false;
		}
		return foundDigit;
	}

	static int BoolValue(bool value)
	{
		if (value)
			return 1;
		return 0;
	}
}
