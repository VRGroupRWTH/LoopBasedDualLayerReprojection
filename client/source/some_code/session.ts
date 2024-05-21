import Connection from "../RemoteRendering/Connection";
import { Display, DisplayType, build_display } from "./display";
import { ImageDecoder, GeometryDecoder } from "./decoder";


import { GeometryFrame, ImageFrame } from "./decoder";

export class Frame
{
    image_frames : [ImageFrame, ImageFrame];
    geometry_frames : [GeometryFrame, GeometryFrame];

    constructor()
    {
        
    }
}



class Session
{
    private canvas : HTMLCanvasElement;
    private display : Display | null = null;
    private connection : Connection | null = null;

    constructor(canvas : HTMLCanvasElement)
    {
        this.canvas = canvas;
    }

    async create(canvas : HTMLCanvasElement) : Promise<boolean>
    {
        let display = build_display(DisplayType.AR);

        if(!await display?.create(canvas))
        {
            return false;
        }

        





        




        return true;
    }

    private on_display_render()
    {
        
    }

    private on_display_close()
    {

    }

}