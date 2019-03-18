import React, { Component } from 'react';
import { fabric } from 'fabric';
import { Get } from '../utils/ajax'; 

class DetectionArea extends Component {
  constructor() {
    super()
    this.state = { preview: null };
  }

  componentDidMount(){
    const { cam } = this.props;
    this.previewWebCam(cam.id);
  }

  previewWebCam(pk) {
    this.setState({ preview: null }, () => {
      Get('/cctv/webcam-capture', { pk }, preview => {
        this.setState({ preview });
      });
    });
  }

  onPreviewLoad({target:preview}){
    const { cam } = this.props;
    const { left, top, width, height} = cam;
    const previewWidth = preview.offsetWidth, previewHeight = preview.offsetHeight;
    console.log(previewWidth, previewHeight);

    // create a wrapper around native canvas element (with id="c")
    var canvas = new fabric.Canvas(this.canvas);
    canvas.setHeight(previewHeight);
    canvas.setWidth(previewWidth);
    this.camWidth = previewWidth;
    this.camHeight = previewHeight;

    // create a rectangle object
    var rect = new fabric.Rect({
      left, top,
      fill: '#0000ff88',
      width: width ? width : 30,
      height: height ? height : 30
    });

    // "add" rectangle onto canvas
    canvas.add(rect);

    canvas.item(0).setControlsVisibility({ mtr: false });

    this.fcanvas = canvas;
  }

  getPosition(){
    var obj = this.fcanvas.getActiveObject() || this.fcanvas.item(0);
    const {camWidth, camHeight} = this;
    return { left: obj.left, top: obj.top, width: obj.width * obj.scaleX, height: obj.height * obj.scaleY, camWidth, camHeight};
  }

  render(){
    const { cam} = this.props;
    const { preview} = this.state;
    return <div>
      <div style={{position: 'absolute' }}>
        <canvas ref={ref => this.canvas = ref} />
      </div>
      {preview && <img ref={ref => this.preview = ref } 
        style={{ width: '100%', height: 'auto'}}
        src={'data:image/png;base64, ' + preview} 
        onLoad={this.onPreviewLoad.bind(this)}/>}
      {/* <img src={"/api/cctv/webcam-capture?_=" + timestamp} style={{width: '100%'}} /> */}
    </div>
  }
}

export default DetectionArea;