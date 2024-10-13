const fs = require('fs');
const path = require('path');
const fse = require('fs-extra');

const sourceDir = path.resolve(__dirname, 'dist', 'browser');
const targetDir = path.resolve(__dirname, '..', 'monitoring');

// Ensure the target directory exists
fse.ensureDirSync(targetDir);

// Clear older build files
fse.emptyDirSync(targetDir);  

// Copy build from dist to the Monitoring cmake folder
fse.copy(sourceDir, targetDir, { overwrite: true }, (err) => {
  if (err) {
    console.error('Error moving build files:', err);
  } else {
    console.log(`Build files successfully moved to: ${targetDir}`);
  }
});