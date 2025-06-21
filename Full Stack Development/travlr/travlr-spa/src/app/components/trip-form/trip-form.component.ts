// src/app/components/trip-form/trip-form.component.ts
import { Component, OnInit } from '@angular/core';
import { FormBuilder, FormGroup, Validators, ReactiveFormsModule } from '@angular/forms';
import { CommonModule } from '@angular/common';
import { ActivatedRoute, Router } from '@angular/router';
import { TripService, Trip } from '../../services/trip.service';

@Component({
  selector: 'app-trip-form',
  standalone: true,
  imports: [CommonModule, ReactiveFormsModule],
  templateUrl: './trip-form.component.html',
  styleUrls: ['./trip-form.component.css']
})
export class TripFormComponent implements OnInit {
  tripForm!: FormGroup;
  isEditMode: boolean = false;
  tripId: string | null = null;

  constructor(
    private fb: FormBuilder,
    private tripService: TripService,
    private route: ActivatedRoute,
    private router: Router
  ) {}

  ngOnInit(): void {
    // Initialize the form with required fields and validators
    this.tripForm = this.fb.group({
      name: ['', Validators.required],
      image: ['', Validators.required],
      description1: ['', Validators.required],
      description2: ['', Validators.required]
    });

    // Check if an 'id' parameter exists in the route for edit mode
    this.tripId = this.route.snapshot.paramMap.get('id');
    if (this.tripId) {
      this.isEditMode = true;
      // Load the existing trip details to pre-fill the form
      this.tripService.getTrip(this.tripId).subscribe((trip) => {
        this.tripForm.patchValue({
          name: trip.name,
          image: trip.image,
          description1: trip.description1,
          description2: trip.description2
        });
      });
    }
  }

  onSubmit(): void {
    if (this.tripForm.invalid) {
      return;
    }

    // Create a trip object from the form values
    const trip: Trip = {
      id: this.tripId ? this.tripId : '',
      name: this.tripForm.value.name,
      image: this.tripForm.value.image,
      description1: this.tripForm.value.description1,
      description2: this.tripForm.value.description2
    };

    // Depending on edit mode, call the proper service method
    if (this.isEditMode) {
      this.tripService.updateTrip(this.tripId!, trip).subscribe(() => {
        // Navigate back to the trip list on successful update
        this.router.navigate(['/trips']);
      });
    } else {
      this.tripService.createTrip(trip).subscribe(() => {
        // Navigate back to the trip list on successful creation
        this.router.navigate(['/trips']);
      });
    }
  }
}
