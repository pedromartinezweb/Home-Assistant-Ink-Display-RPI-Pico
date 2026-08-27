$fn = 48;

part = "plate";
pcb_width = 72;
pcb_height = 30;
pcb_clearance = 1;
case_depth = 20;
corner_radius = 3;
front_thickness = 3;
wall = 2;
case_width = pcb_width + (wall + pcb_clearance) * 2;
case_height = pcb_height + (wall + pcb_clearance) * 2;
window_width = 60;
window_height = 26;
window_radius = 1;
module_hole_x = 66.4;
module_hole_y = 24.4;
boss_diameter = 5;
boss_height = 1.5;
pilot_diameter = 1.6;
lid_clearance = 0.25;
lid_thickness = 2;
lid_plug_height = 1.5;
plate_gap = 3;

module rounded_prism(size, radius) {
    linear_extrude(height = size[2])
        offset(r = radius)
            square([size[0] - radius * 2, size[1] - radius * 2], center = true);
}

module main_body() {
    difference() {
        union() {
            difference() {
                rounded_prism([case_width, case_height, case_depth], corner_radius);
                translate([0, 0, front_thickness])
                    rounded_prism([
                        case_width - wall * 2,
                        case_height - wall * 2,
                        case_depth
                    ], corner_radius - wall / 2);
                translate([0, 0, -0.1])
                    rounded_prism([
                        window_width,
                        window_height,
                        front_thickness + 0.2
                    ], window_radius);
                translate([-case_width / 2 - 0.1, -6, 7])
                    cube([wall + 0.3, 12, 8]);
            }
            for (x = [-module_hole_x / 2, module_hole_x / 2])
                for (y = [-module_hole_y / 2, module_hole_y / 2])
                    translate([x, y, front_thickness - 0.05])
                        cylinder(d = boss_diameter, h = boss_height + 0.05);
        }
        for (x = [-module_hole_x / 2, module_hole_x / 2])
            for (y = [-module_hole_y / 2, module_hole_y / 2])
                translate([x, y, 1])
                    cylinder(d = pilot_diameter, h = front_thickness + boss_height);
    }
}

module zip_slot(x, y) {
    translate([x - 4, y - 1, -0.1])
        cube([8, 2, lid_thickness + lid_plug_height + 0.2]);
}

module lid() {
    difference() {
        union() {
            rounded_prism([case_width, case_height, lid_thickness], corner_radius);
            translate([0, 0, lid_thickness])
                rounded_prism([
                    case_width - wall * 2 - lid_clearance * 2,
                    case_height - wall * 2 - lid_clearance * 2,
                    lid_plug_height
                ], corner_radius - wall / 2);
        }
        for (x = [-18, 18]) {
            zip_slot(x, -13);
            zip_slot(x, 13);
        }
        for (x = [-31, 31])
            for (y = [-10, -5, 0, 5, 10])
                translate([x - 2.5, y - 0.8, -0.1])
                    cube([5, 1.6, lid_thickness + lid_plug_height + 0.2]);
    }
    for (x = [-22, 22])
        for (y = [-8, 8])
            translate([x, y, lid_thickness + lid_plug_height - 0.05])
                cylinder(d = 4, h = 1.05);
}

if (part == "body") {
    main_body();
} else if (part == "lid") {
    lid();
} else {
    main_body();
    translate([0, case_height + plate_gap, 0]) lid();
}
