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
