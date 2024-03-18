import { Component, Index, Suspense, createResource, createSignal } from "solid-js";
import { Button, ListGroup } from "solid-bootstrap";

const Scene : Component<{set_state : (state : string) => void}> = (props) =>
{
    const get_file_name = (file_path : string) =>
    {
        const offset : number = file_path.lastIndexOf("\\");

        return file_path.substring(offset + 1);
    };

    const fetch_scenes = async () =>
    {
        const response = await fetch("http://localhost:3000/server/scenes");
        const scenes = await response.json();

        return scenes;
    };

    const [scenes] = createResource(fetch_scenes);
    const [selected_scene, set_selected_scene] = createSignal(-1);

    return (
        <div>
            <div class="border rounded my-3" style="background-color: rgb(248, 249, 250); max-height: 300px; overflow-y: scroll; overflow-x: hidden;">
                <ListGroup as="ol" variant="flush">
                    <Suspense fallback={<div class="w-100 h-100 d-flex justify-content-center align-items-center"><div class="spinner-border text-secondary" role="status"></div></div>}>
                        <Index each={scenes()}>
                            {(scene, index) =>
                                <ListGroup.Item as="li" class="d-flex justify-content-between align-items-start" action active={index == selected_scene()} onClick={() => set_selected_scene(index)}>   
                                    <div class="ms-2 me-auto">
                                        <div class="fw-bold">{get_file_name(scene())}</div>
                                        <div style="font-size: 10pt;">{scene()}</div>
                                    </div>
                                </ListGroup.Item>
                            }
                        </Index>
                    </Suspense>
                </ListGroup>
            </div>
            <div class="d-flex">
                <Button class="ms-auto" disabled={selected_scene() == -1} onClick={() => props.set_state("settings")}>Next</Button>
            </div>
        </div>
    );
};

export default Scene;