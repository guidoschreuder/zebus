// Width of the PCB
pcb_width = 50.1;

// Length of the PCB
pcb_length = 50.1;

// PCB thickness
pcb_thickness = 1.6;

// Height from top of baseplate to bottom of PCB
pcb_heigth_offset = 3.5;

// Thickness of base plate
baseplate_thickness = 1.5;

// Thickness of the ledge that supports the PCB
pcb_support_width = 2.1;

pcb_support_clear_corner = 2.1;

// Thickness of the ridge that keeps the PCB
ridge_width = 3.7;

// number of slats cut out of baseplate
num_slats = 6;

sides = "none"; // ["none", "front", "back", "both"]

// number of PCB retainment screws per side
num_pcb_retainment_screws_per_side = 1;

// diameter of the PCB retainment screw hole
pcb_retainment_screws_hole_diameter = 3.2;

print_part();

num_sides = (sides == "both") ? 2 : ((sides == "front" || sides == "back") ? 1 : 0);
total_width = pcb_width + 2 * ridge_width;
total_length = pcb_length + num_sides * ridge_width;
retainment_screw_x_offset = .3 * pcb_retainment_screws_hole_diameter;

module screw_body(x, y, diameter, cutout=false) {
    h = baseplate_thickness + pcb_heigth_offset + pcb_thickness;
    dim = cutout ? diameter / 2 : diameter;
    translate([x, y, 0]) 
        cylinder(h,  dim, dim, $fs=.5);
}

module retainment_screws(cutout=false) {
  screw_inc = pcb_length / num_pcb_retainment_screws_per_side;
  x_offset = pcb_retainment_screws_hole_diameter - retainment_screw_x_offset;
  for (i = [1:num_pcb_retainment_screws_per_side]) {
    y = num_pcb_retainment_screws_per_side == 1 ? pcb_length / 2 : (i - .5) * screw_inc;
    screw_body(ridge_width - x_offset, y, pcb_retainment_screws_hole_diameter, cutout);
    screw_body(pcb_width + ridge_width + x_offset, y, pcb_retainment_screws_hole_diameter, cutout);
    }
}

module holder_base() {
    
    // baseplate
    cube([total_width, pcb_length, baseplate_thickness]);
    
    // draw left support
    translate([ridge_width, pcb_support_clear_corner, baseplate_thickness]) 
        cube([pcb_support_width, pcb_length - 2 * pcb_support_clear_corner, pcb_heigth_offset]);
    // draw left ridge
    translate([0, 0, baseplate_thickness]) 
        cube([ridge_width, pcb_length, pcb_heigth_offset + pcb_thickness]);

    // draw right support
    translate([pcb_width - pcb_support_width + ridge_width, pcb_support_clear_corner, baseplate_thickness]) 
      cube([pcb_support_width + ridge_width, pcb_length - 2 * pcb_support_clear_corner, pcb_heigth_offset]);
    // draw right ridge
    translate([pcb_width + ridge_width, 0, baseplate_thickness]) 
      cube([ridge_width, pcb_length, pcb_heigth_offset + pcb_thickness]);
    
    // draw sides
    if (sides == "front" || sides == "both") {
        translate([0, -ridge_width, 0]) 
            cube([total_width, ridge_width, baseplate_thickness + pcb_heigth_offset + pcb_thickness]);
    }
    if (sides == "back" || sides == "both") {
        translate([0, pcb_length, 0]) 
            cube([total_width, ridge_width, baseplate_thickness + pcb_heigth_offset + pcb_thickness]);
    }

}

module holder_slats_cutout() {
    if(num_slats > 0) {
        slat_inc = pcb_length / (2 * num_slats + 1);
        echo(slat_inc=slat_inc);
        for (i = [1:num_slats]) {
            y = (2 * (i - 1) + 1) * slat_inc;
            translate([0, y, 0])
                cube([pcb_width + 2 * pcb_support_width + 2 * ridge_width, slat_inc, baseplate_thickness]);
        }
    }
}

module pcb_cutout() {
    translate([ridge_width, 0, baseplate_thickness + pcb_heigth_offset]) 
        cube([pcb_width, pcb_length, pcb_thickness]);
}

module holder() {    
    difference() {
        union() {
            difference(){
              holder_base();
              holder_slats_cutout();
            }
          retainment_screws();
        }
        retainment_screws(true);
        pcb_cutout();
    }
}

module print_part() {
    translate([-total_width/2, -total_length /2, 0]) holder();
}
