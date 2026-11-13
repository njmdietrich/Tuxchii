# Ocarina of Time: intermittent FSR 4 camera tracking

The reported Water Temple camera-angle dropouts occur on integration build `c2285b35ae`.
The supplied log successfully loads fsr4-vk 4.1.1 INT8 and creates a
2560x1440-to-3840x2160 context. There are no FSR 4 context or dispatch failure messages.

The log contains 234 fallback messages saying `no scene camera this frame` and five
saying `no scene depth this frame`. These are logged events, not a count of all fallback
frames. For example, the game's upscaler draws from frame 5529; FSR 4 evaluates again at
frame 5548 with `reset=true`. A second such gap runs from frame 5570 to frame 5599.

`RasterizerVulkan::TrackOcarinaCamera` looks for a scene-sized B10G11R11 render target
with depth and a vertex-stage camera buffer at binding 3. It rejects GPU-modified buffers,
unexpected camera/target extents and a projection whose checked pair differs from
`(-1, 0)`. `TryOcarinaUpscale` requires a camera and scene depth from the current frame
and channel. A missed camera leaves the game's pass running; resuming after a missing
frame resets the temporal history in `OcarinaUpscaler::Record`.

The supplied log identifies camera acquisition as the main failing gate, but does not
record which camera check rejected the frame. It does not establish that the game's
fallback algorithm is FSR 1. The window's FSR 1 scaling filter is a separate stage.

Upstream has a similar angle-dependent DLSS report in
[Lichii #1579](https://github.com/zortos293/Lichii/issues/1579). Both backends share this
camera acquisition path. The diagnostic changes from
[Lichii #1494](https://github.com/zortos293/Lichii/pull/1494), including its follow-up fix
for throttled long outages, are ported on `investigate/oot-fsr4-camera-loss`.
They report the failed camera check, fallback duration, recovery and any loss of the
4K presentation image. They preserve the existing camera acceptance rules.

To collect the missing evidence, use that branch's Linux x86-64 PGO artifact, retain
the settings from the reported run, and repeat the camera movement across the affected
Water Temple boundary. The ordinary Info-level log contains the new diagnostics;
no extra environment variables or shader capture are needed. The next investigation
should follow the specific rejection reason rather than reuse an old camera or remove
validation without observing the affected scene.

## Diagnostic reproduction and candidate fix

The follow-up log from `e62c1a3eba` records six fallback starts, all with
`no scene camera this frame: no camera buffer bound`. Five have a logged recovery;
the last is still active at the end of the log. For example, frames 1969 through
2093 use the game's upscaler (125 frames, 2.1 seconds). The log separately confirms
that the game's 1440p image replaces the reconstructed 4K image over the same span.
FSR 4 INT8 loads and evaluates successfully.

That diagnostic combines two cases: the vertex camera slot is not visited, or its
shader-visible range is shorter than 560 bytes. `ForEachBoundBuffer` visits only
the current shader's enabled slots and trims each range to the bytes the shader
uses. This is the wrong source of availability for a camera read by the emulator:
which surfaces the camera sees changes the shaders and their uniform usage.
The log does not distinguish those two cases further.

`fix/oot-fsr4-camera-binding` reads the current guest vertex CB3 binding directly.
The range required is 448 bytes (`0x1c0`): the previous view-projection matrix at
`0x180` ends there. The old 560-byte requirement included unused trailing fields.
The candidate retains scene extent, perspective and channel checks, checks the
entire read is mapped and continuous, and refuses GPU-modified data. It does not
reuse the previously accepted camera when a new camera cannot be read.

A one-time `scene camera recovered from guest vertex CB3` message records the
declared and shader-visible byte counts when a camera is accepted that the old
tracker would have missed. Continued failures now distinguish a disabled guest
binding, a short binding, mapping problems and GPU-written data. The pure camera
decoder has regression coverage for the exact 448-byte layout, truncated data,
the two matrix offsets, invalid extents/projections and DLSS padding limits.

This is a candidate fix for the structural detection error. The affected Water
Temple camera route still needs testing on the new build to establish that the
reported interruptions are gone and that the reconstructed image remains correct.
