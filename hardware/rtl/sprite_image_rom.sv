// -----------------------------------------------------------------------------
// sprite_image_rom.sv -- procedural 32x32 placeholder sprite image ROM
//
//  This module provides opaque/RGB pixels for the 30 sprite IDs defined in
//  game/game.h. The art is intentionally simple and fully combinational so the
//  video pipeline can be verified before bitmap/MIF assets are generated.
// -----------------------------------------------------------------------------

`default_nettype none

module sprite_image_rom (
    input  logic [4:0] sprite_id,
    input  logic [4:0] local_x,
    input  logic [4:0] local_y,

    output logic       opaque,
    output logic [7:0] rgb_r,
    output logic [7:0] rgb_g,
    output logic [7:0] rgb_b
);

    localparam logic [4:0] SPRITE_PLAYER1_FRONT            = 5'd0;
    localparam logic [4:0] SPRITE_PLAYER1_BACK             = 5'd1;
    localparam logic [4:0] SPRITE_PLAYER1_LEFT             = 5'd2;
    localparam logic [4:0] SPRITE_PLAYER1_RIGHT            = 5'd3;
    localparam logic [4:0] SPRITE_PLAYER2_FRONT            = 5'd4;
    localparam logic [4:0] SPRITE_PLAYER2_BACK             = 5'd5;
    localparam logic [4:0] SPRITE_PLAYER2_LEFT             = 5'd6;
    localparam logic [4:0] SPRITE_PLAYER2_RIGHT            = 5'd7;
    localparam logic [4:0] SPRITE_PLAYER1_BOMB             = 5'd8;
    localparam logic [4:0] SPRITE_PLAYER2_BOMB             = 5'd9;
    localparam logic [4:0] SPRITE_PLAYER1_EXPLOSION_CENTER = 5'd10;
    localparam logic [4:0] SPRITE_PLAYER2_EXPLOSION_CENTER = 5'd11;
    localparam logic [4:0] SPRITE_PLAYER1_EXPLOSION_UP     = 5'd12;
    localparam logic [4:0] SPRITE_PLAYER1_EXPLOSION_DOWN   = 5'd13;
    localparam logic [4:0] SPRITE_PLAYER2_EXPLOSION_UP     = 5'd14;
    localparam logic [4:0] SPRITE_PLAYER2_EXPLOSION_DOWN   = 5'd15;
    localparam logic [4:0] SPRITE_PLAYER1_EXPLOSION_LEFT   = 5'd16;
    localparam logic [4:0] SPRITE_PLAYER1_EXPLOSION_RIGHT  = 5'd17;
    localparam logic [4:0] SPRITE_PLAYER2_EXPLOSION_LEFT   = 5'd18;
    localparam logic [4:0] SPRITE_PLAYER2_EXPLOSION_RIGHT  = 5'd19;

    logic player1_sprite;
    logic player2_sprite;
    logic player_sprite;
    logic bomb_sprite;
    logic explosion_sprite;
    logic ui_sprite;

    logic body;
    logic outline;
    logic eye;
    logic backpack;
    logic bomb_body;
    logic fuse;
    logic blast_core;
    logic blast_edge;
    logic ui_glyph;

    always_comb begin
        player1_sprite  = (sprite_id >= SPRITE_PLAYER1_FRONT) &&
                          (sprite_id <= SPRITE_PLAYER1_RIGHT);
        player2_sprite  = (sprite_id >= SPRITE_PLAYER2_FRONT) &&
                          (sprite_id <= SPRITE_PLAYER2_RIGHT);
        player_sprite   = player1_sprite || player2_sprite;
        bomb_sprite     = (sprite_id == SPRITE_PLAYER1_BOMB) ||
                          (sprite_id == SPRITE_PLAYER2_BOMB);
        explosion_sprite = (sprite_id >= SPRITE_PLAYER1_EXPLOSION_CENTER) &&
                           (sprite_id <= SPRITE_PLAYER2_EXPLOSION_RIGHT);
        ui_sprite       = (sprite_id >= 5'd20) && (sprite_id <= 5'd29);

        outline   = (local_x >= 5'd6) && (local_x <= 5'd25) &&
                    (local_y >= 5'd4) && (local_y <= 5'd29);
        body      = (local_x >= 5'd8) && (local_x <= 5'd23) &&
                    (local_y >= 5'd6) && (local_y <= 5'd27);
        eye       = ((local_x >= 5'd11) && (local_x <= 5'd13) &&
                     (local_y >= 5'd12) && (local_y <= 5'd14)) ||
                    ((local_x >= 5'd18) && (local_x <= 5'd20) &&
                     (local_y >= 5'd12) && (local_y <= 5'd14));
        backpack  = (local_x >= 5'd10) && (local_x <= 5'd21) &&
                    (local_y >= 5'd8) && (local_y <= 5'd11);

        bomb_body = (local_x >= 5'd8) && (local_x <= 5'd24) &&
                    (local_y >= 5'd10) && (local_y <= 5'd27);
        fuse      = (local_x >= 5'd18) && (local_x <= 5'd23) &&
                    (local_y >= 5'd5) && (local_y <= 5'd10);

        blast_core = ((local_x >= 5'd10) && (local_x <= 5'd21)) ||
                     ((local_y >= 5'd10) && (local_y <= 5'd21));
        blast_edge = ((local_x >= 5'd6) && (local_x <= 5'd25) &&
                      (local_y >= 5'd6) && (local_y <= 5'd25)) &&
                     ((local_x[1:0] == 2'b00) || (local_y[1:0] == 2'b00));

        ui_glyph = (local_x >= 5'd7) && (local_x <= 5'd24) &&
                   (local_y >= 5'd7) && (local_y <= 5'd24) &&
                   ((local_x == local_y) ||
                    (local_x + local_y == 5'd31) ||
                    (local_x[4:2] == sprite_id[2:0]));

        opaque = 1'b0;
        rgb_r  = 8'h00;
        rgb_g  = 8'h00;
        rgb_b  = 8'h00;

        if (player_sprite && outline) begin
            opaque = 1'b1;
            if (!body) begin
                rgb_r = 8'h18;
                rgb_g = 8'h18;
                rgb_b = 8'h1C;
            end else if (eye && (sprite_id == SPRITE_PLAYER1_FRONT ||
                                 sprite_id == SPRITE_PLAYER2_FRONT)) begin
                rgb_r = 8'hFF;
                rgb_g = 8'hFF;
                rgb_b = 8'hFF;
            end else if (backpack && (sprite_id == SPRITE_PLAYER1_BACK ||
                                      sprite_id == SPRITE_PLAYER2_BACK)) begin
                rgb_r = 8'h30;
                rgb_g = 8'h30;
                rgb_b = 8'h36;
            end else if (player1_sprite) begin
                rgb_r = 8'hF4;
                rgb_g = 8'hC7;
                rgb_b = 8'h46;
            end else begin
                rgb_r = 8'hEA;
                rgb_g = 8'h62;
                rgb_b = 8'hA0;
            end
        end else if (bomb_sprite && (bomb_body || fuse)) begin
            opaque = 1'b1;
            if (fuse) begin
                rgb_r = 8'hFF;
                rgb_g = (sprite_id == SPRITE_PLAYER1_BOMB) ? 8'hD6 : 8'h78;
                rgb_b = (sprite_id == SPRITE_PLAYER1_BOMB) ? 8'h3A : 8'hC0;
            end else begin
                rgb_r = 8'h12;
                rgb_g = 8'h14;
                rgb_b = 8'h18;
            end
        end else if (explosion_sprite && (blast_core || blast_edge)) begin
            opaque = 1'b1;
            if (blast_core) begin
                rgb_r = 8'hFF;
                rgb_g = 8'hE8;
                rgb_b = 8'h72;
            end else if (sprite_id[0]) begin
                rgb_r = 8'hFF;
                rgb_g = 8'h74;
                rgb_b = 8'hB0;
            end else begin
                rgb_r = 8'hFF;
                rgb_g = 8'h90;
                rgb_b = 8'h28;
            end
        end else if (ui_sprite && ui_glyph) begin
            opaque = 1'b1;
            if (sprite_id >= 5'd25) begin
                rgb_r = 8'hF4;
                rgb_g = 8'hC7;
                rgb_b = 8'h46;
            end else if (sprite_id >= 5'd22) begin
                rgb_r = 8'hEA;
                rgb_g = 8'h62;
                rgb_b = 8'hA0;
            end else begin
                rgb_r = 8'hF0;
                rgb_g = 8'hF4;
                rgb_b = 8'hFA;
            end
        end
    end

endmodule

`default_nettype wire
