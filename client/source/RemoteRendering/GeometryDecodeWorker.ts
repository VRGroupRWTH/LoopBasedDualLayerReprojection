import * as Wrapper from "../../wrapper/binary/wrapper";

Wrapper.default().then(wrapper => {
    addEventListener("message", event => {
        const t0 = performance.now();
        const result = wrapper.decode_geoemtry(event.data.data);
        if (result.success) {
            const t1 = performance.now();
            // There are two options to transfer data from emscripten
            // If you comment in another alternative, make sure to also change it in the lib.cpp
            // OPTION A
            const vertices = result.vertices as Uint8Array;
            const indices = result.indices as Uint8Array;
            postMessage({
                requestId: event.data.requestId,
                layerIndex: event.data.layerIndex,
                decodeInitiationTime: event.data.decodeInitiationTime,
                rawDecodeTime: t1 - t0,
                vertices: vertices,
                indices: indices,
            }, [indices.buffer, vertices.buffer]);

            // OPTION B
            // const vertices = new Uint8Array(streamingLib.HEAPU8.buffer, result.verticesPointer, result.verticesByteLength);
            // const indices = new Uint8Array(streamingLib.HEAPU8.buffer, result.indicesPointer, result.indicesByteLength);

            // postMessage({
            //     requestId: event.data.requestId,
            //     layerIndex: event.data.layerIndex,
            //     decodeInitiationTime: event.data.decodeInitiationTime,
            //     rawDecodeTime: result.rawDecodeTime,
            //     vertices: vertices,
            //     indices: indices
            // });
        }
    });
});


