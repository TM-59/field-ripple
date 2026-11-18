// Types of the WebAssembly module that `make web` builds as dist/field.mjs from the C sources
// (EXPORTED_FUNCTIONS in the Makefile). Used only by `tsc --checkJs`; nothing is emitted.

/** Pointers are byte offsets into HEAPF64's buffer. */
export interface FieldModule {
  HEAPF64: Float64Array;
  _fr_reset(speed: number): number;
  _fr_step(direction: number, steps: number): number;
  _fr_time(): number;
  _fr_position(): number;
  _fr_velocity(): number;
  _fr_energy(): number;
  _fr_width(): number;
  _fr_stopped(): number;
  _fr_count(): number;
  _fr_samples(): number;
  _lw_reset(): void;
  _lw_command(command: number): void;
  _lw_tick(frames: number): void;
  _lw_answer(option: number): number;
  _lw_value(field: number): number;
  _lw_count(): number;
  _lw_samples(): number;
  _pw_reset(mode: number, param: number): number;
  _pw_step(steps: number): number;
  _pw_value(field: number): number;
  _pw_count(): number;
  _pw_columns(): number;
  _pw_rows(): number;
  _pw_samples(): number;
  _pw_history(): number;
}

export default function createField(): Promise<FieldModule>;
