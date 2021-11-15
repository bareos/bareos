.. blockdiag::

   blockdiag {
     // standard node shapes
     box [shape = box];
     square [shape = square];
     roundedbox [shape = roundedbox];
     dots [shape = dots];

     circle [shape = circle];
     ellipse [shape = ellipse];
     diamond [shape = diamond];
     minidiamond [shape = minidiamond];

     note [shape = note];
     mail [shape = mail];
     cloud [shape = cloud];
     actor [shape = actor];

     beginpoint [shape = beginpoint];
     endpoint [shape = endpoint];

     box -> square -> roundedbox -> dots;
     circle -> ellipse -> diamond -> minidiamond;
     note -> mail -> cloud -> actor;
     beginpoint -> endpoint;

     // node shapes for flowcharts
     condition [shape = flowchart.condition];
     database [shape = flowchart.database];
     terminator [shape = flowchart.terminator];
     input [shape = flowchart.input];

     loopin [shape = flowchart.loopin];
     loopout [shape = flowchart.loopout];

     condition -> database -> terminator -> input;
     loopin -> loopout;
   }
