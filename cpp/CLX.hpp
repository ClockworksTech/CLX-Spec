#include <msgpack11>
#include <cstdint>
#include <string>
#include <vector>

typedef struct Deck {
    double velocity = 0;
    double position = 0;
    double position2 = 0;
    double norm_position = 0;
    double bpm = 0;
    double length = 0;
    float eqlow = 0;
    float eqmid = 0;
    float eqhigh = 0;
    int deck = 0;
    int beat = 0;
    
    static Deck fromMsgPack(const msgpack11::MsgPack& msg) {
        Deck deck;
        deck.velocity = msg["Pitch"].float64_value();
        deck.position = msg["Position"].float64_value();
        deck.position2 = msg["Position2"].float64_value();
        deck.norm_position = msg["NormalizedPosition"].float64_value();
        deck.bpm = msg["BPM"].float64_value();
        deck.length = msg["Length"].float64_value();
        deck.eqlow = msg["EQLow"].float32_value();
        deck.eqmid = msg["EQMid"].float32_value();
        deck.eqhigh = msg["EQHigh"].float32_value();
        deck.deck = msg["Deck"].uint8_value();
        deck.beat = msg["Beat"].int32_value();
        return deck;
    }
        
} Deck_t;

typedef struct Meta {
    int deck;
    std::string title;
    std::string artist;
    std::string album;
    std::string filepath;

    static Meta fromMsgPack(const msgpack11::MsgPack& msg) {
        Meta meta;
        meta.deck = msg["Deck"].uint8_value();
        meta.title = msg["Title"].string_value();
        meta.artist = msg["Artist"].string_value();
        meta.album = msg["Album"].string_value();
        meta.filepath = msg["FilePath"].string_value();
        return meta;
    }
} Meta_t;


typedef struct Control {
    double upfader_a = 0;
    double upfader_b = 0;
    double upfader_c = 0;
    double upfader_d = 0;
    double crossfader = 0;
    int active;
    std::string state = "Disconnected";

    static Control fromMsgPack(const msgpack11::MsgPack& msg) {
        Control control;
        control.upfader_a = msg["UpfaderA"].float64_value();
        control.upfader_b = msg["UpfaderB"].float64_value();
        control.upfader_c = msg["UpfaderC"].float64_value();
        control.upfader_d = msg["UpfaderD"].float64_value();
        control.crossfader = msg["Crossfader"].float64_value();
        control.active = msg["Active"].int32_value();
        control.state = msg["AppState"].string_value();
        return control;
    }
} Control_t;

typedef struct Event {
    std::string event;
    uint8_t value;
    static Event fromMsgPack(const msgpack11::MsgPack& msg) {
        Event event;
        event.event = msg["Event"].string_value();
        event.value = msg["Value"].uint8_value();
        return event;
    }

} Event_t;

// Envelope for a single `0x05` binary fragment. Large payloads are split into
// multiple fragments sharing the same Hash and reassembled in Order. The Type
// field discriminates the reassembled payload (e.g. "waveform", "beatgrid").
typedef struct BinaryMessage {
    std::string type;
    std::vector<uint8_t> hash;
    uint64_t total = 0;
    uint32_t order = 0;
    uint32_t total_packets = 0;
    std::vector<uint8_t> data;

    static BinaryMessage fromMsgPack(const msgpack11::MsgPack& msg) {
        BinaryMessage bin;
        bin.type = msg["Type"].string_value();
        bin.hash = msg["Hash"].binary_items();
        bin.total = msg["Total"].uint64_value();
        bin.order = msg["Order"].uint32_value();
        bin.total_packets = msg["TotalPackets"].uint32_value();
        bin.data = msg["Data"].binary_items();
        return bin;
    }
} BinaryMessage_t;

// Reassembled `0x05` payload with Type == "waveform". Cached as an rwf file.
typedef struct WaveformData {
    std::vector<uint8_t> data;
    std::vector<uint8_t> hash;
    bool replace = false;

    static WaveformData fromMsgPack(const msgpack11::MsgPack& msg) {
        WaveformData wf;
        wf.data = msg["Data"].binary_items();
        wf.hash = msg["Hash"].binary_items();
        wf.replace = msg["Replace"].bool_value();
        return wf;
    }
} WaveformData_t;

// A single beat marker within a beatgrid payload.
typedef struct BeatgridMarker {
    float bpm = 0;
    float position = 0;      // seconds from the start of the track
    bool terminal = false;   // start/end markers that bracket the grid
    uint32_t beats_to_next = 0;

    static BeatgridMarker fromMsgPack(const msgpack11::MsgPack& msg) {
        BeatgridMarker marker;
        marker.bpm = msg["Bpm"].float32_value();
        marker.position = msg["Position"].float32_value();
        marker.terminal = msg["Terminal"].bool_value();
        marker.beats_to_next = msg["BeatsToNext"].uint32_value();
        return marker;
    }
} BeatgridMarker_t;

// Reassembled `0x05` payload with Type == "beatgrid". Cached as a bg file.
typedef struct BeatgridPayload {
    std::vector<uint8_t> hash;   // 32-byte ASCII-hex track hash
    uint32_t total = 0;          // total number of beats in the grid
    std::vector<BeatgridMarker> markers;

    static BeatgridPayload fromMsgPack(const msgpack11::MsgPack& msg) {
        BeatgridPayload grid;
        grid.hash = msg["Hash"].binary_items();
        grid.total = msg["Total"].uint32_value();
        for (const auto& item : msg["Markers"].array_items()) {
            grid.markers.push_back(BeatgridMarker::fromMsgPack(item));
        }
        return grid;
    }
} BeatgridPayload_t;
