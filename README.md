# CLX

# MessagePack Packet Format: DJ Deck State Sync

This document describes the structure of serialized packets used for syncing real-time DJ playback data between clients and servers. All packets are sent over the network as:

```
[1-byte Packet Type Header][MessagePack Payload]
```

---

## MessagePack Schema Notes

Due to hardware memory limitations, MessagePack payloads are assumed to have a schema-less encoding  scheme.  The first and only top-level object is a map of Key-Value pairs, similar to a JSON object.  This is how libraries like `MPack`, `msgpack11`, and `msgpack23` serialize data by default.

This allows for arbitrary field/schema ordering and the ability to add backwards compatability for additional fields without the need for a proper protocol versioning system.  However, due to limited memory constraints, it must be assumed that hardware devices using CLX will only parse the first object of a MessagePack payload, and only if it is a Map of Key/Value objects.  This allows for a much lower-level, recursion-free parsing scheme on hardware using `msgpack-c`.

---

The data is exposed as udp unicast, broadcast, or multicast over port `3650`. If using unicast it is recommended to offer a "hop" from the receiver to another network.

---

## Packet Type Headers

| Byte Value | Packet Type | Description                                  |
|------------|-------------|----------------------------------------------|
| `0x01`     | Deck        | Real-time deck data (sent up to 60fps)       |
| `0x02`     | Meta        | Track metadata (sent on load or event)       |
| `0x00`     | Control     | Mixer and control state                      |
| `0x03`     | Waveform    | Waveform request (2-byte micro-packet)       |
| `0x04`     | Event       | Event trigger (e.g., load, cue, play toggle) |
| `0x05`     | Binary      | Fragmented binary data (waveform, beatgrid, cues) |

---

## Deck Packet (`0x01`)

Represents the real-time state of a single playback deck.

| Key                 | Type     | Description                            |
|---------------------|----------|----------------------------------------|
| `Pitch`             | `float64`| Track pitch or velocity                |
| `Position`          | `float64`| Current playhead position (in seconds) |
| `Position2`         | `float64`| Beatgrid position (`Beat Number.fraction`)       |
| `NormalizedPosition`| `float64`| Position normalized to range [0.0–1.0] |
| `BPM`               | `float64`| Current BPM of the track               |
| `Length`            | `float64`| Track duration (in seconds)            |
| `EQLow`             | `float32`| Low EQ gain level                      |
| `EQMid`             | `float32`| Mid EQ gain level                      |
| `EQHigh`            | `float32`| High EQ gain level                     |
| `Deck`              | `uint8`  | Deck index (e.g., 0 = A, 1 = B)        |
| `Beat`              | `uint8`  | Current Beat (1-4) all other values should be ignored |

---

## Metadata Packet (`0x02`)

Track metadata, typically sent once on load or when requested. All strings UTF-8 encoded.

| Key         | Type       | Description                          |
|-------------|------------|--------------------------------------|
| `Deck`      | `uint8`    | Deck number                          |
| `Title`     | `string`   | Track title                          |
| `Artist`    | `string`   | Track artist                         |
| `Album`     | `string`   | Track album                          |
| `FilePath`  | `string`   | Path to the loaded track file        |

---

## Control Packet (`0x00`)

Represents mixer fader states and app state.

| Key         | Type       | Description                              |
|-------------|------------|------------------------------------------|
| `UpfaderA`  | `float64`  | Fader level for Deck A                   |
| `UpfaderB`  | `float64`  | Fader level for Deck B                   |
| `UpfaderC`  | `float64`  | Fader level for Deck C                   |
| `UpfaderD`  | `float64`  | Fader level for Deck D                   |
| `Crossfader`| `float64`  | Crossfader position (typically 0 to 1)   |
| `Active`    | `uint8`    | Active deck or focus status              |
| `AppState`  | `string`   | App connection or session state          |

---

## Event Packet (`0x04`)

Signals a client-initiated action or state change.

| Key     | Type     | Description                              |
|---------|----------|------------------------------------------|
| `Event` | `string` | Event name (e.g., "Load", "Cue", "Play") |
| `Value` | `uint8`  | Optional numeric value for the event     |

---

## Binary Data (`0x05`)

A binary packet containing arbitrary data. Payloads larger than a single UDP datagram are split into fragments; each fragment is one `0x05` packet carrying the same envelope fields below, and the receiver reassembles them by `Order`. The `Type` field discriminates the payload (`waveform`, `beatgrid`, `cues`). Additional fields are optional depending on how the data needs to be used. It is recommended to include an order value as well as the expected total size.

| Key            | Type     | Description                                              |
|----------------|----------|---------------------------------------------------------|
| `Type`         | `str`    | Payload discriminator: `waveform`, `beatgrid` or `cues` |
| `Hash`         | `bin`    | 32-byte payload identifier (ASCII-hex track hash)       |
| `Total`        | `uint64` | Total size of the reassembled payload in bytes          |
| `Order`        | `uint32` | Fragment order index (0-based)                          |
| `TotalPackets` | `uint32` | Total number of fragments (optional; 0/absent on older senders — receiver then completes on the byte `Total`) |
| `Data`         | `bin`    | Fragment binary data                                    |

We follow the conventions from the BBC, with one alteration, appended to the bottom is a CLRS section in binary containing the rgb color values for each pair of values. This is represented as clrs in the json format.
https://github.com/bbc/audiowaveform/blob/master/doc/DataFormat.md

### Waveform Payload (`Type = "waveform"`)

Waveform data comes in as binary data in fragments. The re-assembled data is a messagepack blob as below. These should be saved as `rwf` files in a local cache.

| Key       | Type     | Description                                                        |
|-----------|----------|-------------------------------------------------------------------|
| `Data`    | `bin`    | The binary of the waveform data                                   |
| `Hash`    | `bin`    | md5 sum of track title, waveform file name                        |
| `Replace` | `bool`   | Optional; if true the receiver overwrites an existing cached waveform for this hash (older senders omit it, defaulting to false) |

### Beatgrid Payload (`Type = "beatgrid"`)

Beatgrid data is delivered over the same `0x05` transport with `Type = "beatgrid"`, fragmented and reassembled exactly like the waveform payload. The re-assembled messagepack blob is as below and should be saved as `bg` files in a local cache.

| Key       | Type     | Description                                    |
|-----------|----------|------------------------------------------------|
| `Hash`    | `bin`    | 32-byte ASCII-hex track hash                   |
| `Total`   | `uint32` | Total number of beats in the grid              |
| `Markers` | `array`  | Array of beatgrid marker maps (see below)      |

Each entry in `Markers` is a map:

| Key           | Type     | Description                                                   |
|---------------|----------|--------------------------------------------------------------|
| `Bpm`         | `float32`| Tempo in effect from this marker                             |
| `Position`    | `float32`| Marker position in seconds from the start of the track      |
| `Terminal`    | `bool`   | True for the start/end markers that bracket the grid        |
| `BeatsToNext` | `uint32` | Number of beats from this marker to the next                |

### Cues Payload (`Type = "cues"`)

The hot cues and memory cues the DJ software holds for a track, delivered over
the same `0x05` transport with `Type = "cues"`, fragmented and reassembled
exactly like the waveform and beatgrid payloads. Keyed by the same track hash
so a receiver can attach all three to one track. The re-assembled messagepack
blob is as below and may be saved as `cues` files in a local cache.

| Key    | Type    | Description                                   |
|--------|---------|-----------------------------------------------|
| `Hash` | `bin`   | 32-byte ASCII-hex track hash (md5 of title)   |
| `Cues` | `array` | Array of cue maps (see below), any order      |

Each entry in `Cues` is a map:

| Key      | Type      | Description                                                        |
|----------|-----------|--------------------------------------------------------------------|
| `Time`   | `float32` | Cue position in seconds from the start of the track                |
| `Name`   | `str`     | Cue label; an empty string when the cue is unnamed (always present, never nil) |
| `Hotcue` | `bool`    | `true` for a hot cue (pad), `false` for a memory cue               |

The sender transmits the full list for a track whenever it has one (typically
right after the track's waveform and beatgrid), and re-sends the full list if it
changes; a receiver replaces whatever it held for that hash rather than merging.
These are the DJ software's own cues, not the receiver's: a viewer displays them
(e.g. as markers on the waveform) and must not treat them as its own cue list.



## Waveform Request (`0x03`)

This is a special form of micro-packet used by clients that support retransmission of a waveform. The payload is exactly two bytes:

```
[0x03 Header][Deck (uint8)]
```

Receiving a request triggers transmission of the currently loaded waveform for that deck on the source. Unlike the state-sync packets on port `3650`, CLX Senders listen for waveform requests on port `7000` specifically.


## Behavioral Notes

- Clients **broadcast a magic packet** upon joining to request full metadata resync (`Meta`, `Control`) `0x09`.
- `Deck` packets are streamed continuously and may be throttled to 60fps for performance.
- `Event` packets are **commands**, not state — the actions they define are user definable.
- Servers are expected to track and respond based on `Deck`, `Meta`, and `Control` states.
- If you are a spectator it is highly recommended to implement both unicast and multicast listen. Multicast happens on address `239.0.0.1`

---
