import { Component, Index, Match, Suspense, Switch, createEffect, createSignal } from "solid-js"
import { receive_scenes } from "../remote_rendering";
import { Button, CloseButton, ListGroup, ListGroupItem, Modal, ModalBody, ModalHeader } from "solid-bootstrap";
import { createAsync } from "@solidjs/router";

export interface SceneBrowserProps
{
    show : () => boolean;
    set_show : (show : boolean) => void;
    set_select: (scene_file : string) => void;
}

interface SceneFile
{
    name : string,
    index : number
}

const SceneBrowser : Component<SceneBrowserProps> = (props) =>
{
    const [search_query, set_search_query] = createSignal<string>();
    const [scenes_filtered, set_scenes_filtered] = createSignal<SceneFile[]>();
    const [scene_selected, set_scene_selected] = createSignal<number>();
    const scenes = createAsync(() => receive_scenes())
    
    createEffect(() =>
    {
        let query = search_query();

        if(query == undefined)
        {
            query = "";   
        }
        
        const files = scenes();
        let files_filtered : SceneFile[] = [];

        if(files != undefined)
        {
            for(let index = 0; index < files.length; index++)
            {
                const file = files[index];

                if(!file.includes(query))   
                {
                    continue;
                }

                const name = file.substring(file.lastIndexOf("/") + 1);

                const file_filtered = 
                {
                    name,
                    index
                }

                files_filtered.push(file_filtered);   
            }
        }

        set_scenes_filtered(files_filtered);
    });

    function on_select()
    {
        const files = scenes();
        const file_index = scene_selected();

        if(files != undefined && file_index != undefined)
        {
            props.set_select(files[file_index]);
        }

        props.set_show(false);
    }

    function on_search(target: HTMLInputElement)
    {
        if(target.value != "")
        {
            set_search_query(target.value);   
        }

        else
        {
            set_search_query(undefined);
        }
    }

    function is_selected(file_index : number) : boolean
    {
        if(scene_selected() == undefined)
        {
            return false;
        }

        return file_index == scene_selected();
    }

    function is_empty()
    {
        const files = scenes_filtered()

        if(files == undefined)
        {
            return true;   
        }

        return files.length == 0
    }

    return (
        <Modal size="lg" show={props.show()}>
            <ModalHeader class="text-bg-dark">
                <h5 class="m-0" style="font-size: 1.25rem">Select Scene</h5>
                <CloseButton variant="white" onclick={event => props.set_show(false)}></CloseButton>
            </ModalHeader>
            <ModalBody>
                <input class="form-control search-input" placeholder="Search" onInput={event => on_search(event.target)}></input>
                <div class="my-3">
                    <Suspense fallback=
                        {
                            <div class="border rounded d-flex align-items-center justify-content-center overflow-y-scroll" style="height: 300px">
                                <h5 style="font-size: 1.25rem">Loading...</h5>
                            </div>
                        }>
                        <Switch>
                            <Match when={!is_empty()}>
                                <div class="border rounded overflow-hidden">
                                    <div class="overflow-y-scroll" style="max-height: 300px">
                                        <ListGroup variant="flush">
                                            <Index each={scenes_filtered()}>
                                                {(scene_file, index) => 
                                                    <ListGroupItem action active={is_selected(scene_file().index)} onClick={event => set_scene_selected(index)} ondblclick={event => on_select()}>
                                                        {scene_file().name}
                                                    </ListGroupItem>
                                                }
                                            </Index>
                                        </ListGroup>
                                    </div>
                                </div>
                            </Match>
                            <Match when={is_empty()}>
                                <div class="border rounded d-flex align-items-center justify-content-center overflow-y-scroll" style="height: 300px">
                                    <h5 style="font-size: 1.25rem">No Scenes found</h5>
                                </div>
                            </Match>
                        </Switch>
                    </Suspense>
                </div>
                <div class="d-flex justify-content-end">
                    <Button class="btn btn-primary" disabled={scene_selected() == undefined} onClick={event => on_select()}>Select</Button>
                </div>
            </ModalBody>
        </Modal>
    );
};

export default SceneBrowser;