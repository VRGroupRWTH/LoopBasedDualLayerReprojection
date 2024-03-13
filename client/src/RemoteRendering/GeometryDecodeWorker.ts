console.log("hello form worker");
import createARStreamingLib from "../../lib/build/ar-streaming-lib";
createARStreamingLib().then(streamingLib => {
    addEventListener("message", event => {
        const result = streamingLib.decodeGeometry(event.data.data);
        if (result) {
            // There are two options to transfer data from emscripten
            // If you comment in another alternative, make sure to also change it in the lib.cpp
            // OPTION A
            const vertices = new Uint8Array(result.vertices.buffer);
            const indices = new Uint8Array(result.indices.buffer);
            postMessage({
                requestId: event.data.requestId,
                layerIndex: event.data.layerIndex,
                decodeInitiationTime: event.data.decodeInitiationTime,
                rawDecodeTime: result.rawDecodeTime,
                vertices: vertices,
                indices: indices,
            }, [result.vertices.buffer, result.indices.buffer]);

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


