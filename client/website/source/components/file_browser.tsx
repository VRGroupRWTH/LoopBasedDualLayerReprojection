import { createAsync } from "@solidjs/router";
import { Button, CloseButton, Form, ListGroup, ListGroupItem, Modal, ModalBody, ModalHeader } from "solid-bootstrap";
import { Component, Index, Match, Suspense, Switch, createSignal } from "solid-js";
import { receive_directory_files } from "../remote_rendering";
import { DirectoryFileType } from "../remote_rendering/connection";

export interface FileBrowserProps
{
    select_type : DirectoryFileType;
    show : () => boolean;
    set_show : (show : boolean) => void;
    set_select: (file : string) => void;
}

const FileBrowser : Component<FileBrowserProps> = (props) =>
{
    const [directory, set_directory] = createSignal("/");
    const [file_selected, set_file_selected] = createSignal<number>();
    const files = createAsync(async () =>
    {
        const files = await receive_directory_files(directory());

        files.sort((file1, file2) =>
        {
            if(file1.type == "directory" && file2.type == "file")
            {
                return -1;   
            }

            if(file1.type == "file" && file2.type == "directory")
            {
                return 1;   
            }

            return file1.name.localeCompare(file2.name);
        });

        return files;
    });

    function on_upwards()
    {
        const current_directory = directory();

        if(current_directory == "/")
        {
            return;
        }

        const offset = current_directory.lastIndexOf("/", current_directory.length - 2);

        if(offset == -1)
        {
            return;   
        }

        set_directory(current_directory.substring(0, offset + 1));
        set_file_selected(undefined);
    }

    function on_select()
    {
        const current_files = files();
        const current_index = file_selected();

        if(current_index == undefined || current_files == undefined)
        {
            return;   
        }

        const file = current_files[current_index];

        if(file.type == "directory")
        {
            set_directory(directory() + file.name + "/");
            set_file_selected(undefined);
        }

        else if(file.type == "file")
        {
            props.set_select(directory() + file.name);
            props.set_show(false);
        }
    }

    function is_selected(file_index : number) : boolean
    {
        if(file_selected() == undefined)
        {
            return false;
        }

        return file_index == file_selected();
    }

    function is_empty()
    {
        const current_files = files()

        if(current_files == undefined)
        {
            return true;   
        }

        return current_files.length == 0
    }

    return (
        <Modal size="lg" show={props.show()} >
            <ModalHeader class="text-bg-dark">
                <Switch>
                    <Match when={props.select_type == "directory"}>
                        <h5 class="m-0" style="font-size: 1.25rem">Select Directory</h5>
                    </Match>
                    <Match when={props.select_type == "file"}>
                        <h5 class="m-0" style="font-size: 1.25rem">Select File</h5>
                    </Match>
                </Switch>
                <CloseButton variant="white" onclick={event => props.set_show(false)}></CloseButton>
            </ModalHeader>
            <ModalBody>
                <div class="d-flex">
                    <Button class="d-flex align-items-center justify-content-center me-2" style="width: 38px; height: 38px;" onClick={event => on_upwards()}><img src="../assets/icons/directory_up.svg"></img></Button>
                    <Form.Control class="flex-fill" type="text" value={directory()} disabled></Form.Control>
                </div>
                <div class="my-3">
                    <Suspense fallback={<div>Loading...</div>}>
                        <Switch>
                            <Match when={!is_empty()}>
                                <div class="border rounded overflow-hidden">
                                    <div class="overflow-y-scroll" style="max-height: 300px">
                                        <ListGroup variant="flush">
                                            <Index each={files()}>
                                                {(file, index) => 
                                                    <ListGroupItem class="d-flex align-items-center" action active={is_selected(index)} onClick={event => set_file_selected(index)}>
                                                        <Switch>
                                                            <Match when={file().type == "file"}><img src="../assets/icons/file.svg"></img></Match>
                                                            <Match when={file().type == "directory"}><img src="../assets/icons/directory.svg"></img></Match>
                                                        </Switch>
                                                        <div class="ms-2">
                                                            {file().name}
                                                        </div>
                                                    </ListGroupItem>
                                                }
                                            </Index>
                                        </ListGroup>
                                    </div>
                                </div>
                            </Match>
                            <Match when={is_empty()}>
                                <div class="border rounded d-flex align-items-center justify-content-center overflow-y-scroll" style="height: 300px">
                                    <h5 style="font-size: 1.25rem">Directory Empty!</h5>
                                </div>
                            </Match>
                        </Switch>
                    </Suspense>
                </div>
                <div class="d-flex justify-content-end">
                    <Button class="btn btn-primary" disabled={file_selected() == undefined} onClick={event => on_select()}>Select</Button>
                </div>
            </ModalBody>
        </Modal>
    );
}

export default FileBrowser;