const XAM = require("@rnbo/xam");
const fs = require("fs-extra");

fs.ensureDirSync("./patchers");

let a = [];

// make the basic test patcher

XAM.setAdditionalOptions({ audiocodegen : true });
XAM.startRecordingObjectCreation(a);

let param1 = XAM.createObject("param", { name : "testparam" });

XAM.stopRecordingObjectCreation();

let patch = XAM.codegen.compile(a, {
    classname : "rnbomaticOneParam",
    presets : true
});

let patchStr = XAM.cppcodegen.generate(patch, {
    classname : "rnbomatic"
});

fs.writeFileSync("./patchers/rnbomaticOneParam.cpp", patchStr);

// minimal engine test file
XAM.setAdditionalOptions({ audiocodegen : true });
XAM.startRecordingObjectCreation(a);

param1 = XAM.createObject("param", { name : "testparam" });

XAM.stopRecordingObjectCreation();

patch = XAM.codegen.compile(a, {
    classname : "rnbomaticMinimal",
    presets : true
});

patchStr = XAM.cppcodegen.generate(patch, {
    classname : "rnbomaticMinimal"
});

fs.writeFileSync("./patchers/rnbomaticMinimal.cpp", patchStr);

// core engine test file
XAM.setAdditionalOptions({ audiocodegen : true });
XAM.startRecordingObjectCreation(a);

param1 = XAM.createObject("param", {
    name : "testparam",
    min : -2,
    max : 23,
    value : 12
});

XAM.stopRecordingObjectCreation();

patch = XAM.codegen.compile(a, {
    classname : "rnbomaticEngineCore",
    presets : true
});

patchStr = XAM.cppcodegen.generate(patch, {
    classname : "rnbomaticEngineCore"
});

fs.writeFileSync("./patchers/rnbomaticEngineCore.cpp", patchStr);

// core engine test file
XAM.setAdditionalOptions({ audiocodegen : true });
XAM.startRecordingObjectCreation(a);

param1 = XAM.createObject("param", {
    name : "testparam1",
    value : 0.23
});

param1 = XAM.createObject("param", {
    name : "testparam2",
    value : 0.1
});

XAM.stopRecordingObjectCreation();

patch = XAM.codegen.compile(a, {
    classname : "rnbomaticEngine",
    presets : true
});

patchStr = XAM.cppcodegen.generate(patch, {
    classname : "rnbomaticEngine"
});

fs.writeFileSync("./patchers/rnbomaticEngine.cpp", patchStr);
