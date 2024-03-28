import Layer from "./Layer";

class Mesh {
    constructor(readonly requestId: number, public layers: Layer[]) {
    }

    complete() {
        for (const layer of this.layers) {
            if (!layer.complete()) {
                return false;
            }
        }
        return true;
    }
}

export default Mesh;
